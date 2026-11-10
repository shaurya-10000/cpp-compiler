#pragma once
#include "ast.h"
#include "sema.h"
#include "types.h"
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Codegen {
public:
    Codegen(std::ostream& out, const Sema& sema);
    void generate(Program& program);

private:
    std::ostream& out_;
    std::ostream* cur_;   // current destination (function body buffer or out_)
    const Sema& sema_;
    std::vector<std::unordered_map<std::string, int>> scopes_;
    int current_offset_ = 0;
    int push_depth_ = 0;  // 8-byte slots pushed beyond the frame, for call alignment
    int label_count_ = 0;
    int string_count_ = 0;
    std::vector<std::pair<std::string, std::string>> strings_;
    Type current_ret_ = Type::Void;

    std::string current_break_label_;
    std::string current_continue_label_;

    std::ostream& emit() { return *cur_; }
    Type type_of(const Expr* e) const;

    void emit_builtins();
    void emit_rodata();
    void gen_function(FuncDecl& fn);

    void push_scope();
    void pop_scope();
    void allocate_var(const std::string& name);
    int lookup_var(const std::string& name);

    void emit_convert(Type from, Type to);
    void emit_to_bool_from_double();
    void gen_double_binary(Tok op);
    void gen_cond(Expr* e);   // evaluates e and leaves flags for je/jne (ZF set when false)

    void gen_stmt(Stmt* s);
    void gen_expr(Expr* e);
    void gen_block(Block* b);
    void gen_call(Call* call);
    std::string new_label();
    std::string add_string(const std::string& s);
};