#pragma once
#include "ast.h"
#include <string>

struct DoubleLit : Expr {
    double value = 0.0;
};

struct CastExpr : Expr {
    std::string type;   // target type name, may include '*'
    ExprPtr operand;
};

struct AddrOfExpr : Expr {   // &operand
    ExprPtr operand;
};

struct DerefExpr : Expr {    // *operand
    ExprPtr operand;
};

struct IndexExpr : Expr {    // base[index]
    ExprPtr base;
    ExprPtr index;
};

struct StoreExpr : Expr {    // *p = v  /  a[i] = v
    ExprPtr target;          // DerefExpr or IndexExpr
    ExprPtr value;
};

struct ArrayVarDecl : VarDecl {   // T name[length];  (type holds the element type)
    long long length = 0;
};