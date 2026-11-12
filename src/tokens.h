#pragma once
#include <string>

enum class Tok {
    INT_LIT, DOUBLE_LIT, CHAR_LIT, STRING_LIT, IDENT,
    KW_INT, KW_BOOL, KW_CHAR, KW_VOID, KW_DOUBLE,
    KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_DO, KW_RETURN,
    KW_BREAK, KW_CONTINUE, KW_TRUE, KW_FALSE, KW_SIZEOF, KW_STRUCT,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, NE, LT, GT, LE, GE,
    ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN, PERCENT_ASSIGN,
    AMP_ASSIGN, PIPE_ASSIGN, CARET_ASSIGN, SHL_ASSIGN, SHR_ASSIGN,
    AMP, PIPE, CARET, TILDE, SHL, SHR,
    AND, OR, NOT,
    PLUSPLUS, MINUSMINUS,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    COMMA, SEMICOLON, QUESTION, COLON, DOT, ARROW,
    END,
    // Added after v1.0.0; appended so the numeric values of existing tokens do not change.
    KW_ENUM, KW_SWITCH, KW_CASE, KW_DEFAULT, KW_TYPEDEF
};

struct Token {
    Tok type;
    std::string text;
    long long int_val = 0;
    char char_val = 0;
    int line = 1;
    int col = 1;
    double dbl_val = 0.0;
};
