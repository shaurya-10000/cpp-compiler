#pragma once
#include "ast.h"
#include "tokens.h"
#include <memory>
#include <string>
#include <vector>

struct DoubleLit : Expr {
    double value = 0.0;
};

struct CastExpr : Expr {
    std::string type;   // target type name, may include '*'
    ExprPtr operand;
};

struct AddrOfExpr : Expr { ExprPtr operand; };      // &operand
struct DerefExpr : Expr { ExprPtr operand; };       // *operand

struct IndexExpr : Expr {                           // base[index]
    ExprPtr base;
    ExprPtr index;
};

struct StoreExpr : Expr {                           // *p = v, a[i] = v, s.x = v
    ExprPtr target;
    ExprPtr value;
};

struct MemberExpr : Expr {                          // base.name  /  base->name
    ExprPtr base;
    std::string name;
    bool arrow = false;
};

struct CompoundExpr : Expr {                        // target op= value   (op is the base operator)
    Tok op = Tok::PLUS;
    ExprPtr target;
    ExprPtr value;
};

struct IncDecExpr : Expr {                          // ++x  x++  --x  x--
    ExprPtr target;
    bool inc = true;
    bool prefix = true;
};

struct CondExpr : Expr {                            // cond ? then_e : else_e
    ExprPtr cond;
    ExprPtr then_e;
    ExprPtr else_e;
};

struct ArrayInit {
    bool has_list = false;
    bool has_str = false;
    std::vector<ExprPtr> list;
    std::string str;
};

struct ArrayVarDecl : VarDecl {                     // T name[length];  (type = element type)
    long long length = 0;                           // -1 means "infer from initializer"
    ArrayInit init_data;
};

struct DoWhile : Stmt {
    std::unique_ptr<Block> body;
    ExprPtr cond;
};

struct GlobalDecl {
    int line = 0;
    std::string type;       // element type for arrays
    std::string name;
    bool is_array = false;
    long long length = 0;   // -1 means "infer from initializer"
    ExprPtr init;           // scalar initializer
    ArrayInit init_data;    // array initializer
};
using Globals = std::vector<std::unique_ptr<GlobalDecl>>;