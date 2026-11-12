#include "ast_dump.h"
#include "codegen.h"
#include "lexer.h"
#include "optimizer.h"
#include "parser.h"
#include "peephole.h"
#include "preprocessor.h"
#include "prelude.h"
#include "sema.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char* const kVersion = "1.0.0";

void usage(std::ostream& os) {
    os << "minicc " << kVersion << " - a small C-like compiler targeting x86-64 Linux\n"
          "\n"
          "Usage: minicc [options] <mode> <file>\n"
          "\n"
          "Modes:\n"
          "  --compile       emit x86-64 assembly (AT&T syntax)\n"
          "  --dump-tokens   print the token stream\n"
          "  --dump-ast      print the parsed AST\n"
          "  --dump-typed    print the AST after semantic analysis (with types)\n"
          "  --dump-opt      print the AST after optimization (with types)\n"
          "\n"
          "Options:\n"
          "  -o <file>       write the assembly to <file> instead of stdout (--compile only)\n"
          "  -O0 | -O1       disable / enable the optimizer (default: -O1)\n"
          "  --stats         print optimizer statistics to stderr\n"
          "  -DNAME[=VALUE]  define a preprocessor macro (default value 1)\n"
          "  -I<dir>         add a directory searched by #include \"...\"\n"
          "  --version       print the version and exit\n"
          "  -h, --help      show this help\n"
          "\n"
          "Example:\n"
          "  minicc --compile prog.cpp -o prog.s && gcc prog.s -o prog && ./prog\n";
}

std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\n') { out.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += c;
    }
    out.push_back(cur);
    return out;
}

// Prints "file:line[:col]: error: message" followed by the offending source line and a caret.
void report(const std::string& file, const std::string& src, const std::string& what) {
    int line = 0, col = 0;
    std::smatch m;
    if (std::regex_search(what, m, std::regex("line ([0-9]+)"))) line = std::stoi(m[1].str());
    if (std::regex_search(what, m, std::regex("col ([0-9]+)"))) col = std::stoi(m[1].str());

    std::string msg = what;
    if (what.rfind("Parse error", 0) == 0 || what.rfind("Sema error line", 0) == 0 ||
        what.rfind("Preprocess error line", 0) == 0) {
        const size_t p = what.find(": ");
        if (p != std::string::npos) msg = what.substr(p + 2);
    }

    std::cerr << file;
    if (line > 0) {
        std::cerr << ":" << line;
        if (col > 0) std::cerr << ":" << col;
    }
    std::cerr << ": error: " << msg << "\n";

    if (line <= 0) return;
    const std::vector<std::string> lines = split_lines(src);
    if (line > static_cast<int>(lines.size())) return;

    const std::string& text = lines[static_cast<size_t>(line - 1)];
    const std::string num = std::to_string(line);
    std::cerr << " " << num << " | " << text << "\n";
    if (col > 0) {
        std::string pad;
        for (int i = 0; i < col - 1; ++i)
            pad += (static_cast<size_t>(i) < text.size() && text[static_cast<size_t>(i)] == '\t') ? '\t' : ' ';
        std::cerr << " " << std::string(num.size(), ' ') << " | " << pad << "^\n";
    }
}

// Errors carry a line number in the preprocessed text; map it back to the file and line it came from.
void locate_error(const Preprocessor& pp, std::string& what, std::string& file, std::string& src) {
    static const std::regex re("line ([0-9]+)");
    std::smatch m;
    if (!std::regex_search(what, m, re)) return;
    Preprocessor::Origin o;
    if (!pp.origin(std::stoi(m[1].str()), o)) return;
    const std::string fixed = m.prefix().str() + "line " + std::to_string(o.line) + m.suffix().str();
    what = fixed;
    file = pp.file_name(o.file);
    src = pp.file_text(o.file);
}

}  // namespace

int main(int argc, char** argv) {
    std::string mode;
    std::string filepath;
    std::string outpath;
    int opt_level = 1;
    bool stats = false;
    std::vector<std::string> defines;
    std::vector<std::string> include_dirs;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(std::cout); return 0; }
        if (a == "--version") { std::cout << "minicc " << kVersion << "\n"; return 0; }
        if (a == "-o") {
            if (i + 1 >= argc) { std::cerr << "option '-o' requires a file name\n"; return 1; }
            outpath = argv[++i];
        }
        else if (a == "-O0") opt_level = 0;
        else if (a == "-O1" || a == "-O") opt_level = 1;
        else if (a == "-I") {
            if (i + 1 >= argc) { std::cerr << "option '-I' requires a directory\n"; return 1; }
            include_dirs.push_back(argv[++i]);
        }
        else if (a.size() > 2 && a.compare(0, 2, "-I") == 0) include_dirs.push_back(a.substr(2));
        else if (a.size() > 2 && a.compare(0, 2, "-D") == 0) defines.push_back(a.substr(2));
        else if (a == "--stats") stats = true;
        else if (a == "--compile" || a == "--dump-tokens" || a == "--dump-ast" ||
                 a == "--dump-typed" || a == "--dump-opt") mode = a;
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option '" << a << "'\n";
            usage(std::cerr);
            return 1;
        } else if (filepath.empty()) filepath = a;
        else {
            std::cerr << "unexpected argument '" << a << "'\n";
            return 1;
        }
    }
    if (mode.empty() || filepath.empty()) { usage(std::cerr); return 1; }

    std::ifstream f(filepath);
    if (!f) { std::cerr << "cannot open " << filepath << "\n"; return 1; }
    std::stringstream ss; ss << f.rdbuf(); std::string src = ss.str();

    Preprocessor pp;
    for (const auto& dir : include_dirs) pp.add_include_dir(dir);
    for (const auto& def : defines) {
        const size_t eq = def.find('=');
        if (eq == std::string::npos) pp.define(def, "1");
        else pp.define(def.substr(0, eq), def.substr(eq + 1));
    }

    try {
        const std::string preprocessed = pp.run(filepath, src);
        Lexer lex(preprocessed);
        auto toks = lex.tokenize();

        if (mode == "--dump-tokens") {
            std::cout << "Tokens: " << toks.size() << "\n";
            for (auto& t : toks) {
                std::cout << "  type=" << (int)t.type << " text='" << t.text
                          << "' line=" << t.line << "\n";
            }
            return 0;
        }

        Parser p(std::move(toks));
        auto prog = p.parse();

        if (mode == "--dump-ast") {
            dump_program(prog, p.globals(), std::cout, nullptr, 0);
            return 0;
        }

        // Library functions written in minicc are prepended to the user's program.
        Lexer prelude_lex(kPrelude);
        Parser prelude_parser(prelude_lex.tokenize());
        auto prelude = prelude_parser.parse();
        const size_t prelude_count = prelude.functions.size();
        prog.functions.insert(prog.functions.begin(),
                              std::make_move_iterator(prelude.functions.begin()),
                              std::make_move_iterator(prelude.functions.end()));

        Sema sema;
        sema.analyze(prog, p.globals());

        if (mode == "--dump-typed") {
            dump_program(prog, p.globals(), std::cout, &sema, prelude_count);
            return 0;
        }

        OptStats ostats;
        if (opt_level > 0) {
            Optimizer optimizer(sema);
            optimizer.run(prog);
            ostats = optimizer.stats();
        }

        if (mode == "--dump-opt") {
            dump_program(prog, p.globals(), std::cout, &sema, prelude_count);
            if (stats) {
                std::cerr << "[opt] folded=" << ostats.folded << " simplified=" << ostats.simplified
                          << " branches=" << ostats.branches << " dead_stmts=" << ostats.dead_stmts << "\n";
            }
            return 0;
        }

        // mode == "--compile"
        std::ostringstream asm_text;
        Codegen codegen(asm_text, sema);
        codegen.generate(prog, p.globals());

        std::string text = asm_text.str();
        PeepStats pstats;
        if (opt_level > 0) text = peephole(text, pstats);

        if (outpath.empty()) {
            std::cout << text;
        } else {
            std::ofstream of(outpath, std::ios::binary);
            if (!of) { std::cerr << "cannot write " << outpath << "\n"; return 1; }
            of << text;
        }

        if (stats) {
            std::cerr << "[opt]      folded=" << ostats.folded << " simplified=" << ostats.simplified
                      << " branches=" << ostats.branches << " dead_stmts=" << ostats.dead_stmts << "\n";
            std::cerr << "[peephole] removed_lines=" << pstats.removed_lines
                      << " push_pop=" << pstats.push_pop << " fused_branches=" << pstats.branches
                      << " immediates=" << pstats.immediates << " dead=" << pstats.dead << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::string what = e.what();
        std::string efile = filepath;
        std::string esrc = src;
        locate_error(pp, what, efile, esrc);
        report(efile, esrc, what);
        return 1;
    }
}
