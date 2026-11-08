#include "lexer.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    std::string src;
    if (argc > 1) {
        std::ifstream f(argv[1]);
        if (!f) { std::cerr << "cannot open " << argv[1] << "\n"; return 1; }
        std::stringstream ss; ss << f.rdbuf(); src = ss.str();
    } else {
        std::stringstream ss; ss << std::cin.rdbuf(); src = ss.str();
    }
    try {
        Lexer lex(src);
        auto toks = lex.tokenize();
        std::cout << "Tokens: " << toks.size() << "\n";
        for (auto& t : toks) {
            std::cout << "  type=" << (int)t.type << " text='" << t.text
                      << "' line=" << t.line << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "LexError: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
