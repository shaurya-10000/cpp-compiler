#pragma once
#include "ast.h"
#include "tokens.h"
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> toks);
    Program parse();
private:
    std::vector<Token> toks_;
    size_t pos_ = 0;

    const Token& peek(int off = 0) const;
    const Token& advance();
    bool check(Tok t) const;
    bool match(Tok t);
    const Token& expect(Tok t, const char* what);
    [[noreturn]] void error(const std::string& msg) const;

    std::unique_ptr<FuncDecl> parse_function();
    std::unique_ptr<Block> parse_block();
    StmtPtr parse_stmt();
    StmtPtr parse_var_decl();
    StmtPtr parse_if();
    StmtPtr parse_while();
    StmtPtr parse_for();
    StmtPtr parse_return();

    ExprPtr parse_expr();
    ExprPtr parse_assign();
    ExprPtr parse_or();
    ExprPtr parse_and();
    ExprPtr parse_equality();
    ExprPtr parse_relational();
    ExprPtr parse_additive();
    ExprPtr parse_multiplicative();
    ExprPtr parse_unary();
    ExprPtr parse_postfix();
    ExprPtr parse_primary();
};

