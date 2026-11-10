#pragma once
#include "ast.h"
#include <string>

// Phase 8 AST additions
struct DoubleLit : Expr {
    double value = 0.0;
};

struct CastExpr : Expr {
    std::string type;   // target type name: int, bool, char, double
    ExprPtr operand;
};