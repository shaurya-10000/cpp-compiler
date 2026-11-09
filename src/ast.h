#pragma once
#include "tokens.h"
#include <memory>
#include <string>
#include <vector>

// ---------- Expressions ----------
struct Expr {
    int line = 0;
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct IntLit : Expr   { long long value; };
struct CharLit : Expr  { char value; };
struct BoolLit : Expr  { bool value; };
struct VarRef : Expr   { std::string name; };
struct Unary : Expr    { Tok op; ExprPtr operand; };
struct Binary : Expr   { Tok op; ExprPtr lhs, rhs; };
struct Assign : Expr   { std::string name; ExprPtr value; }; 
struct Call : Expr     { std::string callee; std::vector<ExprPtr> args; };

// ---------- Statements ----------
struct Stmt {
    int line = 0;
    virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct Block : Stmt { std::vector<StmtPtr> stmts; };
struct VarDecl : Stmt { std::string type, name; ExprPtr init; };
struct ExprStmt : Stmt { ExprPtr expr; };
struct If : Stmt { ExprPtr cond; std::unique_ptr<Block> then_body, else_body; };
struct While : Stmt { ExprPtr cond; std::unique_ptr<Block> body; };
struct Return : Stmt { ExprPtr value; };

struct Param { std::string type, name; };
struct FuncDecl : Stmt {
    std::string return_type, name;
    std::vector<Param> params;
    std::unique_ptr<Block> body;
};

struct Program {
    std::vector<std::unique_ptr<FuncDecl>> functions;
};
