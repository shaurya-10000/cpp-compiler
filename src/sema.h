#pragma once
#include "ast.h"
#include <string>
#include <unordered_map>
#include <vector>

struct VarInfo {
    std::string type;
    int stack_offset;
};

struct FuncInfo {
    std::string return_type;
    std::vector<std::string> param_types;
    bool defined = false;
};

class Sema {
public:
    Sema();
    void analyze(Program& program);

private:
    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
    std::unordered_map<std::string, FuncInfo> functions_;
    int current_offset_ = 0;
    int loop_depth_ = 0;
    std::string current_function_;

    void push_scope();
    void pop_scope();
    void declare_var(const std::string& name, const std::string& type, int line);
    VarInfo* lookup_var(const std::string& name, int line);
    void declare_function(FuncDecl& fn);

    void check_stmt(Stmt* s);
    void check_expr(Expr* e);
    void check_block(Block* b);
};