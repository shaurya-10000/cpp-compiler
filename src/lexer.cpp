#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

Lexer::Lexer(std::string src) : src_(std::move(src)) {}

char Lexer::peek(int off) const {
    size_t i = pos_ + off;
    return i < src_.size() ? src_[i] : '\0';
}
char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; } else col_++;
    return c;
}
bool Lexer::match(char c) { if (peek() == c) { advance(); return true; } return false; }

void Lexer::skip_ws_and_comments() {
    while (pos_ < src_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { advance(); }
        else if (c == '/' && peek(1) == '/') {
            while (pos_ < src_.size() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            advance(); advance();
            while (pos_ < src_.size() && !(peek() == '*' && peek(1) == '/')) advance();
            if (pos_ < src_.size()) { advance(); advance(); }
        } else break;
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    static const std::unordered_map<std::string, Tok> kws = {
        {"int", Tok::KW_INT}, {"bool", Tok::KW_BOOL}, {"char", Tok::KW_CHAR},
        {"void", Tok::KW_VOID}, {"double", Tok::KW_DOUBLE},
        {"if", Tok::KW_IF}, {"else", Tok::KW_ELSE},
        {"while", Tok::KW_WHILE}, {"for", Tok::KW_FOR},
        {"return", Tok::KW_RETURN}, {"break", Tok::KW_BREAK},
        {"continue", Tok::KW_CONTINUE}, {"break", Tok::KW_BREAK},
        {"true", Tok::KW_TRUE}, {"false", Tok::KW_FALSE},
    };
    while (true) {
        skip_ws_and_comments();
        if (pos_ >= src_.size()) break;
        int l = line_, c = col_;
        char ch = peek();

        if (std::isalpha(ch) || ch == '_') {
            std::string s;
            while (std::isalnum(peek()) || peek() == '_') s += advance();
            auto it = kws.find(s);
            out.push_back(Token{ it == kws.end() ? Tok::IDENT : it->second, s, 0, 0, l, c });
        } else if (std::isdigit(ch)) {
            std::string s;
            while (std::isdigit(peek())) s += advance();
            out.push_back(Token{ Tok::INT_LIT, s, std::stoll(s), 0, l, c });
        } else if (ch == '"') {
            // Parse string literal
            advance(); // skip opening quote
            std::string s;
            while (pos_ < src_.size() && peek() != '"') {
                if (peek() == '\\') {
                    advance();
                    char esc = advance();
                    if (esc == 'n') s += '\n';
                    else if (esc == 't') s += '\t';
                    else s += esc;
                } else {
                    s += advance();
                }
            }
            if (pos_ >= src_.size()) throw std::runtime_error("unterminated string at line " + std::to_string(l));
            advance(); // skip closing quote
            out.push_back(Token{ Tok::STRING_LIT, s, 0, 0, l, c });
        } else if (ch == '\'') {
            // Parse char literal
            advance(); // skip opening quote
            char val = advance();
            if (peek() != '\'') throw std::runtime_error("unterminated char at line " + std::to_string(l));
            advance(); // skip closing quote
            out.push_back(Token{ Tok::CHAR_LIT, std::string(1, val), 0, val, l, c });
        } else {
            char first = advance();
            auto mk = [&](Tok t) {
                out.push_back(Token{ t, std::string(1, first), 0, 0, l, c });
            };
            switch (first) {
                case '+': mk(Tok::PLUS); break;
                case '-': mk(Tok::MINUS); break;
                case '*': mk(Tok::STAR); break;
                case '/': mk(Tok::SLASH); break;
                case '%': mk(Tok::PERCENT); break;
                case '=': mk(match('=') ? Tok::EQ : Tok::ASSIGN); break;
                case '!': mk(match('=') ? Tok::NE : Tok::NOT); break;
                case '<': mk(match('=') ? Tok::LE : Tok::LT); break;
                case '>': mk(match('=') ? Tok::GE : Tok::GT); break;
                case '~': mk(Tok::TILDE); break;
                case '&':
                    if (!match('&')) throw std::runtime_error("unexpected '&' at line " + std::to_string(l));
                    mk(Tok::AND); break;
                case '|':
                    if (!match('|')) throw std::runtime_error("unexpected '|' at line " + std::to_string(l));
                    mk(Tok::OR); break;
                case '(': mk(Tok::LPAREN); break;
                case ')': mk(Tok::RPAREN); break;
                case '{': mk(Tok::LBRACE); break;
                case '}': mk(Tok::RBRACE); break;
                case ';': mk(Tok::SEMICOLON); break;
                case '&': mk(Tok::AND); break;
                case '|': mk(Tok::OR); break;
                case ',': mk(Tok::COMMA); break;
                default:
                    throw std::runtime_error(std::string("unexpected char '") + first + "' at line " + std::to_string(l));
            }
        }
    }
    out.push_back(Token{ Tok::END, "", 0, 0, line_, col_ });
    return out;
}


