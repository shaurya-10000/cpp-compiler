#include "lexer.h"
#include "parser.h"
#include "prelude.h"
#include "sema.h"
#include "codegen.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: minicc [--dump-tokens|--dump-ast|--compile] <file>\n";
        return 1;
    }
    std::string mode = argv[1];
    std::string filepath = argv[2];

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
            std::cout << "AST parsed successfully. (Dump removed for brevity in Phase 2)\n";
            return 0;
        }

        if (mode == "--compile") {
            // Library functions written in minicc are prepended to the user's program.
            Lexer prelude_lex(kPrelude);
            Parser prelude_parser(prelude_lex.tokenize());
            auto prelude = prelude_parser.parse();
            prog.functions.insert(prog.functions.begin(),
                                  std::make_move_iterator(prelude.functions.begin()),
                                  std::make_move_iterator(prelude.functions.end()));

            Sema sema;
            sema.analyze(prog, p.globals());
            Codegen codegen(std::cout, sema);
            codegen.generate(prog, p.globals());
            return 0;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}