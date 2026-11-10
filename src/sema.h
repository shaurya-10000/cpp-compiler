#pragma once
#include "ast.h"
#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

struct VarInfo {
    Type type;
    int stack_offset;
};

struct FuncInfo {
    Type ret = Type::Void;
    std::vector<Type> params;
    bool defined = false;
};

class Sema {
public:
    using TypeMap = std::unordered_map<const Expr*, Type>;
    using FuncMap = std::unordered_map<std::string, FuncInfo>;

    Sema();
    void analyze(Program& program);

    const TypeMap& expr_types() const { return types_; }
    const FuncMap& functions() const { return functions_; }

private:
    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
    FuncMap functions_;
    TypeMap types_;
    int current_offset_ = 0;
    int loop_depth_ = 0;
    Type current_ret_ = Type::Void;
    std::string current_function_;

    void push_scope();
    void pop_scope();
    void declare_var(const std::string& name, Type type, int line);
    VarInfo* lookup_var(const std::string& name, int line);
    void declare_function(FuncDecl& fn);
    Type note(const Expr* e, Type t) { types_[e] = t; return t; }
    void require_value(Type t, int line);

    void check_stmt(Stmt* s);
    Type check_expr(Expr* e);
    void check_block(Block* b);
};