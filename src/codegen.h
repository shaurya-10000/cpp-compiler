#pragma once
#include "ast.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <ostream>

class Codegen {
public:
    Codegen(std::ostream& out);
    void generate(Program& program);

private:
    std::ostream& out_;
    std::vector<std::unordered_map<std::string, int>> scopes_;
    int current_offset_ = 0;
    int label_count_ = 0;
    int string_count_ = 0;
    std::vector<std::pair<std::string, std::string>> strings_;

    void push_scope();
    void pop_scope();
    void allocate_var(const std::string& name);
    int lookup_var(const std::string& name);

    void gen_stmt(Stmt* s);
    void gen_expr(Expr* e);
    void gen_block(Block* b);
    std::string new_label();
    std::string add_string(const std::string& s);
};
