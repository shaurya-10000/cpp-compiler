#include "ast_dump.h"
#include "codegen.h"
#include "lexer.h"
#include "optimizer.h"
#include "parser.h"
#include "peephole.h"
#include "prelude.h"
#include "sema.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

namespace {
void usage() {
    std::cerr <<
        "Usage: minicc [options] <mode> <file>\n"
        "Modes:\n"
        "  --compile       emit x86-64 assembly (AT&T syntax) on stdout\n"
        "  --dump-tokens   print the token stream\n"
        "  --dump-ast      print the parsed AST\n"
        "  --dump-typed    print the AST after semantic analysis (with types)\n"
        "  --dump-opt      print the AST after optimization (with types)\n"
        "Options:\n"
        "  -O0 | -O1       disable / enable the optimizer (default: -O1)\n"
        "  --stats         print optimizer statistics to stderr\n";
}
}  // namespace

int main(int argc, char** argv) {
    std::string mode;
    std::string filepath;
    int opt_level = 1;
    bool stats = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-O0") opt_level = 0;
        else if (a == "-O1" || a == "-O") opt_level = 1;
        else if (a == "--stats") stats = true;
        else if (a == "--compile" || a == "--dump-tokens" || a == "--dump-ast" ||
                 a == "--dump-typed" || a == "--dump-opt") mode = a;
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option '" << a << "'\n";
            usage();
            return 1;
        } else if (filepath.empty()) filepath = a;
        else {
            std::cerr << "unexpected argument '" << a << "'\n";
            return 1;
        }
    }
    if (mode.empty() || filepath.empty()) { usage(); return 1; }

    std::ifstream f(filepath);
    if (!f) { std::cerr << "cannot open " << filepath << "\n"; return 1; }
    std::stringstream ss; ss << f.rdbuf(); std::string src = ss.str();

    try {
        Lexer lex(src);
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
        std::cout << text;

        if (stats) {
            std::cerr << "[opt]      folded=" << ostats.folded << " simplified=" << ostats.simplified
                      << " branches=" << ostats.branches << " dead_stmts=" << ostats.dead_stmts << "\n";
            std::cerr << "[peephole] removed_lines=" << pstats.removed_lines
                      << " push_pop=" << pstats.push_pop << " fused_branches=" << pstats.branches
                      << " immediates=" << pstats.immediates << " dead=" << pstats.dead << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}