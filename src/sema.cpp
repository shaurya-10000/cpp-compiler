#include "sema.h"
#include <stdexcept>

namespace {
[[noreturn]] void fail(int line, const std::string& msg) {
    throw std::runtime_error("Sema error line " + std::to_string(line) + ": " + msg);
}
}

Sema::Sema() { push_scope(); }

void Sema::push_scope() { scopes_.emplace_back(); }
void Sema::pop_scope() { scopes_.pop_back(); }

void Sema::declare_var(const std::string& name, const std::string& type, int line) {
    auto& scope = scopes_.back();
    if (scope.count(name)) fail(line, "variable '" + name + "' already declared in this scope");
    current_offset_ += 8;
    scope[name] = {type, current_offset_};
}

VarInfo* Sema::lookup_var(const std::string& name, int line) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    fail(line, "undeclared variable '" + name + "'");
}

void Sema::declare_function(FuncDecl& fn) {
    if (fn.name == "print_int" || fn.name == "print_str")
        fail(fn.line, "cannot redefine built-in function '" + fn.name + "'");

    FuncInfo info;
    info.return_type = fn.return_type;
    for (auto& p : fn.params) info.param_types.push_back(p.type);
    info.defined = (fn.body != nullptr);

    auto it = functions_.find(fn.name);
    if (it == functions_.end()) {
        functions_[fn.name] = std::move(info);
        return;
    }
    FuncInfo& prev = it->second;
    if (prev.return_type != info.return_type || prev.param_types != info.param_types)
        fail(fn.line, "conflicting declaration of '" + fn.name + "'");
    if (prev.defined && info.defined)
        fail(fn.line, "redefinition of '" + fn.name + "'");
    if (info.defined) prev.defined = true;
}

void Sema::analyze(Program& program) {
    functions_.clear();
    for (auto& fn : program.functions) {
        declare_function(*fn);
        if (!fn->body) continue;

        current_function_ = fn->name;
        current_offset_ = 0;
        loop_depth_ = 0;
        scopes_.clear();
        push_scope();
        for (auto& p : fn->params) declare_var(p.name, p.type, fn->line);
        check_block(fn->body.get());
    }
    auto m = functions_.find("main");
    if (m == functions_.end() || !m->second.defined)
        throw std::runtime_error("Sema error: no 'main' function defined");
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
    } else if (auto* es = dynamic_cast<ExprStmt*>(s)) {
        check_expr(es->expr.get());
    } else if (auto* r = dynamic_cast<Return*>(s)) {
        if (r->value) check_expr(r->value.get());
    } else if (auto* i = dynamic_cast<If*>(s)) {
        check_expr(i->cond.get());
        check_block(i->then_body.get());
        if (i->else_body) check_block(i->else_body.get());
    } else if (auto* w = dynamic_cast<While*>(s)) {
        check_expr(w->cond.get());
        loop_depth_++;
        check_block(w->body.get());
        loop_depth_--;
    } else if (auto* f = dynamic_cast<For*>(s)) {
        push_scope();
        if (f->init) check_stmt(f->init.get());
        if (f->cond) check_expr(f->cond.get());
        if (f->step) check_expr(f->step.get());
        loop_depth_++;
        check_block(f->body.get());
        loop_depth_--;
        pop_scope();
    } else if (dynamic_cast<Break*>(s) != nullptr) {
        if (loop_depth_ == 0) fail(s->line, "'break' outside of loop");
    } else if (dynamic_cast<Continue*>(s) != nullptr) {
        if (loop_depth_ == 0) fail(s->line, "'continue' outside of loop");
    } else if (auto* blk = dynamic_cast<Block*>(s)) {
        check_block(blk);
    }
}

void Sema::check_expr(Expr* e) {
    if (auto* v = dynamic_cast<VarRef*>(e)) {
        lookup_var(v->name, v->line);
    } else if (auto* u = dynamic_cast<Unary*>(e)) {
        check_expr(u->operand.get());
    } else if (auto* bin = dynamic_cast<Binary*>(e)) {
        check_expr(bin->lhs.get());
        check_expr(bin->rhs.get());
    } else if (auto* a = dynamic_cast<Assign*>(e)) {
        lookup_var(a->name, a->line);
        check_expr(a->value.get());
    } else if (auto* call = dynamic_cast<Call*>(e)) {
        if (call->callee == "print_int") {
            if (call->args.size() != 1)
                fail(call->line, "'print_int' expects 1 argument, got " + std::to_string(call->args.size()));
        } else if (call->callee == "print_str") {
            if (call->args.size() != 1 || dynamic_cast<StringLit*>(call->args[0].get()) == nullptr)
                fail(call->line, "'print_str' expects a single string literal argument");
        } else {
            auto it = functions_.find(call->callee);
            if (it == functions_.end())
                fail(call->line, "call to undeclared function '" + call->callee + "'");
            size_t want = it->second.param_types.size();
            if (want != call->args.size())
                fail(call->line, "function '" + call->callee + "' expects " + std::to_string(want) +
                                 " arguments, got " + std::to_string(call->args.size()));
        }
        for (auto& arg : call->args) check_expr(arg.get());
    }
    // literals need no checking
}