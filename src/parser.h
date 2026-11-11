#pragma once
#include "ast.h"
#include "ast_ext.h"
#include "tokens.h"
#include <string>
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> toks);
    Program parse();
    Globals& globals() { return globals_; }

private:
    std::vector<Token> toks_;
    size_t pos_ = 0;
    Globals globals_;

    const Token& peek(int off = 0) const;
    const Token& advance();
    bool check(Tok t) const;
    bool match(Tok t);
    const Token& expect(Tok t, const char* what);
    [[noreturn]] void error(const std::string& msg) const;

    bool is_type_kw(Tok t) const;
    bool is_type_start() const;
    bool is_lvalue(const Expr* e) const;
    bool looks_like_cast() const;
    std::string parse_type();
    void parse_struct_decl();
    void parse_array_suffix(long long& length, ArrayInit& ai);
    void parse_global_rest(const std::string& type, const std::string& name, int line);

    std::unique_ptr<FuncDecl> parse_function_rest(const std::string& ret, const std::string& name, int line);
    std::unique_ptr<Block> parse_block();
    StmtPtr parse_stmt();
    StmtPtr parse_var_decl();
    StmtPtr parse_if();
    StmtPtr parse_while();
    StmtPtr parse_do_while();
    StmtPtr parse_for();
    StmtPtr parse_return();

    ExprPtr parse_expr();
    ExprPtr parse_assign();
    ExprPtr parse_ternary();
    ExprPtr parse_binary(size_t level);
    ExprPtr parse_unary();
    ExprPtr parse_postfix();
    ExprPtr parse_primary();
};