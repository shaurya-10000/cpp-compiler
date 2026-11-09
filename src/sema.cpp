#include "sema.h"
#include <iostream>
#include <stdexcept>

Sema::Sema() { push_scope(); }

void Sema::push_scope() { scopes_.emplace_back(); }
void Sema::pop_scope() { scopes_.pop_back(); }

void Sema::declare_var(const std::string& name, const std::string& type, int line) {
    auto& scope = scopes_.back();
    if (scope.count(name)) {
        throw std::runtime_error("Sema error line " + std::to_string(line) + ": variable '" + name + "' already declared in this scope.");
    }
    current_offset_ += 8; // All vars are 8-byte slots for simplicity in this version
    scope[name] = {type, current_offset_};
}

VarInfo* Sema::lookup_var(const std::string& name, int line) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->count(name)) return &(*it)[name];
    }
    throw std::runtime_error("Sema error line " + std::to_string(line) + ": undeclared variable '" + name + "'.");
}

void Sema::analyze(Program& program) {
    for (auto& fn : program.functions) {
        current_function_ = fn->name;
        current_offset_ = 0;
        scopes_.clear();
        push_scope();

        for (auto& p : fn->params) declare_var(p.name, p.type, fn->line);
        
        check_block(fn->body.get());
        
        // Attach total stack size to the block for codegen (hack: store in name for now or add a field)
        // We'll just re-walk in codegen. For now, this is just a validation pass.
    }
}

void Sema::check_block(Block* b) {
    push_scope();
    for (auto& s : b->stmts) check_stmt(s.get());
    pop_scope();
}

void Sema::check_stmt(Stmt* s) {
    if (auto* v = dynamic_cast<VarDecl*>(s)) {
        if (v->init) check_expr(v->init.get());
        declare_var(v->name, v->type, v->line);
    } else if (auto* e = dynamic_cast<ExprStmt*>(s)) {
        check_expr(e->expr.get());
    } else if (auto* r = dynamic_cast<Return*>(s)) {
        if (r->value) check_expr(r->value.get());
    } else if (auto* i = dynamic_cast<If*>(s)) {
        check_expr(i->cond.get());
        check_block(i->then_body.get());
        if (i->else_body) check_block(i->else_body.get());
    } else if (auto* w = dynamic_cast<While*>(s)) {
        check_expr(w->cond.get());
        check_block(w->body.get());
    } else if (auto* b = dynamic_cast<Block*>(s)) {
        check_block(b);
    }
}

void Sema::check_expr(Expr* e) {
    if (dynamic_cast<IntLit*>(e) || dynamic_cast<CharLit*>(e) || dynamic_cast<BoolLit*>(e)) return;
    if (auto* v = dynamic_cast<VarRef*>(e)) {
        lookup_var(v->name, v->line);
    } else if (auto* u = dynamic_cast<Unary*>(e)) {
        check_expr(u->operand.get());
    } else if (auto* b = dynamic_cast<Binary*>(e)) {
        check_expr(b->lhs.get());
        check_expr(b->rhs.get());
    } else if (auto* a = dynamic_cast<Assign*>(e)) {
        lookup_var(a->name, a->line);
        check_expr(a->value.get());
    } else if (auto* c = dynamic_cast<Call*>(e)) {
        // Note: In a full compiler we'd check args against a function table here.
        for (auto& arg : c->args) check_expr(arg.get());
    }
}
