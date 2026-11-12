#pragma once
#include "ast.h"
#include "ast_ext.h"
#include "tokens.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    std::unordered_map<std::string, std::string> typedefs_;    // alias -> canonical type name
    std::unordered_map<std::string, long long> enum_consts_;   // enumerator -> value
    std::unordered_set<std::string> enum_tags_;

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
    void define_struct_body(const std::string& name);
    void parse_enum_decl();
    void define_enum_body(const std::string& tag);
    bool at_enum_decl() const;
    void parse_typedef();
    std::string typedef_anon_struct_name() const;
    std::string expect_decl_name(const char* what);
    StmtPtr parse_switch();
    StmtPtr parse_case_label();
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
