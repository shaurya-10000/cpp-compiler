#pragma once
#include "tokens.h"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(std::string src);
    std::vector<Token> tokenize();
private:
    std::string src_;
    size_t pos_ = 0;
    int line_ = 1, col_ = 1;
    char peek(int off = 0) const;
    char advance();
    bool match(char c);
    void skip_ws_and_comments();
};
