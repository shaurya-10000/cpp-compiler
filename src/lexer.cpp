#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace {
bool is_digit(char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; }
bool is_alpha(char ch) { return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_'; }
bool is_alnum(char ch) { return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_'; }
}

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
        {"continue", Tok::KW_CONTINUE}, {"sizeof", Tok::KW_SIZEOF},
        {"true", Tok::KW_TRUE}, {"false", Tok::KW_FALSE},
    };
    while (true) {
        skip_ws_and_comments();
        if (pos_ >= src_.size()) break;
        int l = line_, c = col_;
        char ch = peek();

        if (is_alpha(ch)) {
            std::string s;
            while (is_alnum(peek())) s += advance();
            auto it = kws.find(s);
            out.push_back(Token{ it == kws.end() ? Tok::IDENT : it->second, s, 0, 0, l, c });
        } else if (is_digit(ch)) {
            std::string s;
            bool is_float = false;
            while (is_digit(peek())) s += advance();
            if (peek() == '.' && is_digit(peek(1))) {
                is_float = true;
                s += advance();
                while (is_digit(peek())) s += advance();
            }
            if (peek() == 'e' || peek() == 'E') {
                char n1 = peek(1);
                bool signed_exp = (n1 == '+' || n1 == '-') && is_digit(peek(2));
                if (is_digit(n1) || signed_exp) {
                    is_float = true;
                    s += advance();
                    if (signed_exp) s += advance();
                    while (is_digit(peek())) s += advance();
                }
            }
            if (is_float) {
                out.push_back(Token{ Tok::DOUBLE_LIT, s, 0, 0, l, c, std::stod(s) });
            } else {
                long long v = 0;
                try { v = std::stoll(s); }
                catch (const std::exception&) {
                    throw std::runtime_error("integer literal too large at line " + std::to_string(l));
                }
                out.push_back(Token{ Tok::INT_LIT, s, v, 0, l, c });
            }
        } else if (ch == '"') {
            advance();
            std::string s;
            while (pos_ < src_.size() && peek() != '"') {
                if (peek() == '\\') {
                    advance();
                    char esc = advance();
                    if (esc == 'n') s += '\n';
                    else if (esc == 't') s += '\t';
                    else if (esc == 'r') s += '\r';
                    else if (esc == '0') s += '\0';
                    else s += esc;
                } else {
                    s += advance();
                }
            }
            if (pos_ >= src_.size()) throw std::runtime_error("unterminated string at line " + std::to_string(l));
            advance();
            out.push_back(Token{ Tok::STRING_LIT, s, 0, 0, l, c });
        } else if (ch == '\'') {
            advance();
            char val = advance();
            if (val == '\\') {
                char esc = advance();
                val = esc == 'n' ? '\n' : esc == 't' ? '\t' : esc == 'r' ? '\r' : esc == '0' ? '\0' : esc;
            }
            if (peek() != '\'') throw std::runtime_error("unterminated char at line " + std::to_string(l));
            advance();
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
                case '&': mk(match('&') ? Tok::AND : Tok::AMP); break;
                case '|':
                    if (!match('|')) throw std::runtime_error("unexpected '|' at line " + std::to_string(l));
                    mk(Tok::OR); break;
                case '(': mk(Tok::LPAREN); break;
                case ')': mk(Tok::RPAREN); break;
                case '{': mk(Tok::LBRACE); break;
                case '}': mk(Tok::RBRACE); break;
                case '[': mk(Tok::LBRACKET); break;
                case ']': mk(Tok::RBRACKET); break;
                case ';': mk(Tok::SEMICOLON); break;
                case ',': mk(Tok::COMMA); break;
                default:
                    throw std::runtime_error(std::string("unexpected char '") + first + "' at line " + std::to_string(l));
            }
        }
    }
    out.push_back(Token{ Tok::END, "", 0, 0, line_, col_ });
    return out;
}