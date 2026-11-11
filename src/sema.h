#pragma once
#include "ast.h"
#include "ast_ext.h"
#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

struct VarInfo {
    Type type;                 // for arrays: pointer to the element type (decayed)
    bool is_array = false;
    long long array_len = 0;
};

struct FuncInfo {
    Type ret = ty::Void;
    std::vector<Type> params;
    bool defined = false;
};

class Sema {
public:
    using TypeMap = std::unordered_map<const Expr*, Type>;
    using FuncMap = std::unordered_map<std::string, FuncInfo>;

    Sema();
    void analyze(Program& program, Globals& globals);

    const TypeMap& expr_types() const { return types_; }
    const FuncMap& functions() const { return functions_; }

    // Used by the optimizer to register the type of nodes it creates.
    void set_type(const Expr* e, Type t) { types_[e] = t; }

private:
    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
    std::unordered_map<std::string, VarInfo> globals_;
    FuncMap functions_;
    TypeMap types_;
    int loop_depth_ = 0;
    Type current_ret_ = ty::Void;
    std::string current_function_;

    void push_scope();
    void pop_scope();
    void declare_var(const std::string& name, Type type, int line, bool is_array = false, long long len = 0);
    VarInfo* lookup_var(const std::string& name, int line);
    void declare_function(FuncDecl& fn);
    Type note(const Expr* e, Type t) { types_[e] = t; return t; }

    void require_value(Type t, int line);
    void check_object_type(Type t, const char* what, const std::string& name, int line);
    bool convertible(Type from, Type to, const Expr* e) const;
    void require_convertible(Type from, Type to, const Expr* e, int line);
    void check_modifiable(Expr* target);
    void check_array_init(ArrayInit& ai, Type elem, long long& len, int line, const std::string& name);
    void check_global(GlobalDecl& g);
    bool is_const_init(const Expr* e) const;

    void check_stmt(Stmt* s);
    Type check_expr(Expr* e);
    void check_block(Block* b);
};