#pragma once
#include "ast.h"
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Codegen {
public:
    explicit Codegen(std::ostream& out);
    void generate(Program& program);

private:
    std::ostream& out_;
    std::ostream* cur_;   // where instructions currently go (function body buffer or out_)
    std::vector<std::unordered_map<std::string, int>> scopes_;
    int current_offset_ = 0;
    int push_depth_ = 0;  // number of 8-byte pushes beyond the frame, tracked for call alignment
    int label_count_ = 0;
    int string_count_ = 0;
    std::vector<std::pair<std::string, std::string>> strings_;

    std::string current_break_label_;
    std::string current_continue_label_;

    std::ostream& emit() { return *cur_; }

    void emit_builtins();
    void emit_rodata();
    void gen_function(FuncDecl& fn);

    void push_scope();
    void pop_scope();
    void allocate_var(const std::string& name);
    int lookup_var(const std::string& name);

    void gen_stmt(Stmt* s);
    void gen_expr(Expr* e);
    void gen_block(Block* b);
    void gen_call(Call* call);
    std::string new_label();
    std::string add_string(const std::string& s);
};