#include "sema.h"
#include "ast_ext.h"
#include <stdexcept>

namespace {
[[noreturn]] void fail(int line, const std::string& msg) {
    throw std::runtime_error("Sema error line " + std::to_string(line) + ": " + msg);
}
std::string plural(size_t n) { return std::to_string(n) + (n == 1 ? " argument" : " arguments"); }
bool is_builtin_name(const std::string& n) {
    return n == "print_int" || n == "print_str" || n == "print_char";
}
}

Sema::Sema() { push_scope(); }

void Sema::push_scope() { scopes_.emplace_back(); }
void Sema::pop_scope() { scopes_.pop_back(); }

void Sema::declare_var(const std::string& name, Type type, int line) {
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

void Sema::require_value(Type t, int line) {
    if (t == Type::Void) fail(line, "void value used in an expression");
    if (t == Type::Str) fail(line, "string literals can only be used as print_str arguments");
}

void Sema::declare_function(FuncDecl& fn) {
    if (is_builtin_name(fn.name))
        fail(fn.line, "cannot redefine built-in function '" + fn.name + "'");

    FuncInfo info;
    info.ret = type_from_name(fn.return_type);
    for (auto& p : fn.params) info.params.push_back(type_from_name(p.type));
    info.defined = (fn.body != nullptr);

    auto it = functions_.find(fn.name);
    if (it == functions_.end()) {
        functions_[fn.name] = std::move(info);
        return;
    }
    FuncInfo& prev = it->second;
    if (prev.ret != info.ret || prev.params != info.params)
        fail(fn.line, "conflicting declaration of '" + fn.name + "'");
    if (prev.defined && info.defined)
        fail(fn.line, "redefinition of '" + fn.name + "'");
    if (info.defined) prev.defined = true;
}

void Sema::analyze(Program& program) {
    functions_.clear();
    types_.clear();

    FuncInfo builtin;
    builtin.ret = Type::Void;
    builtin.params = {Type::Int};
    builtin.defined = true;
    functions_["print_int"] = builtin;
    functions_["print_char"] = builtin;

    for (auto& fn : program.functions) {
        declare_function(*fn);
        if (!fn->body) continue;

        current_function_ = fn->name;
        current_ret_ = type_from_name(fn->return_type);
        current_offset_ = 0;
        loop_depth_ = 0;
        scopes_.clear();
        push_scope();
        for (auto& p : fn->params) declare_var(p.name, type_from_name(p.type), fn->line);
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
        Type t = type_from_name(v->type);
        if (t == Type::Void) fail(v->line, "variable '" + v->name + "' cannot have type void");
        if (v->init) require_value(check_expr(v->init.get()), v->line);
        declare_var(v->name, t, v->line);
    } else if (auto* es = dynamic_cast<ExprStmt*>(s)) {
        check_expr(es->expr.get());
    } else if (auto* r = dynamic_cast<Return*>(s)) {
        if (r->value) {
            Type t = check_expr(r->value.get());
            if (current_ret_ == Type::Void) fail(r->line, "void function should not return a value");
            require_value(t, r->line);
        } else if (current_ret_ != Type::Void) {
            fail(r->line, "non-void function must return a value");
        }
    } else if (auto* i = dynamic_cast<If*>(s)) {
        require_value(check_expr(i->cond.get()), i->line);
        check_block(i->then_body.get());
        if (i->else_body) check_block(i->else_body.get());
    } else if (auto* w = dynamic_cast<While*>(s)) {
        require_value(check_expr(w->cond.get()), w->line);
        loop_depth_++;
        check_block(w->body.get());
        loop_depth_--;
    } else if (auto* f = dynamic_cast<For*>(s)) {
        push_scope();
        if (f->init) check_stmt(f->init.get());
        if (f->cond) require_value(check_expr(f->cond.get()), f->line);
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

Type Sema::check_expr(Expr* e) {
    if (dynamic_cast<IntLit*>(e)) return note(e, Type::Int);
    if (dynamic_cast<CharLit*>(e)) return note(e, Type::Char);
    if (dynamic_cast<BoolLit*>(e)) return note(e, Type::Bool);
    if (dynamic_cast<DoubleLit*>(e)) return note(e, Type::Double);
    if (dynamic_cast<StringLit*>(e)) return note(e, Type::Str);

    if (auto* v = dynamic_cast<VarRef*>(e)) {
        return note(e, lookup_var(v->name, v->line)->type);
    }
    if (auto* c = dynamic_cast<CastExpr*>(e)) {
        require_value(check_expr(c->operand.get()), c->line);
        return note(e, type_from_name(c->type));
    }
    if (auto* u = dynamic_cast<Unary*>(e)) {
        Type t = check_expr(u->operand.get());
        require_value(t, u->line);
        switch (u->op) {
            case Tok::MINUS: return note(e, t == Type::Double ? Type::Double : Type::Int);
            case Tok::NOT:   return note(e, Type::Bool);
            case Tok::TILDE:
                if (t == Type::Double) fail(u->line, "operator '~' requires an integer operand");
                return note(e, Type::Int);
            default: fail(u->line, "unsupported unary operator");
        }
    }
    if (auto* b = dynamic_cast<Binary*>(e)) {
        Type l = check_expr(b->lhs.get());
        Type r = check_expr(b->rhs.get());
        require_value(l, b->line);
        require_value(r, b->line);
        bool dbl = (l == Type::Double || r == Type::Double);
        switch (b->op) {
            case Tok::PLUS: case Tok::MINUS: case Tok::STAR: case Tok::SLASH:
                return note(e, dbl ? Type::Double : Type::Int);
            case Tok::PERCENT:
                if (dbl) fail(b->line, "operator '%' requires integer operands");
                return note(e, Type::Int);
            case Tok::EQ: case Tok::NE: case Tok::LT: case Tok::GT:
            case Tok::LE: case Tok::GE: case Tok::AND: case Tok::OR:
                return note(e, Type::Bool);
            default: fail(b->line, "unsupported binary operator");
        }
    }
    if (auto* a = dynamic_cast<Assign*>(e)) {
        VarInfo* vi = lookup_var(a->name, a->line);
        require_value(check_expr(a->value.get()), a->line);
        return note(e, vi->type);
    }
    if (auto* call = dynamic_cast<Call*>(e)) {
        if (call->callee == "print_str") {
            if (call->args.size() != 1 || dynamic_cast<StringLit*>(call->args[0].get()) == nullptr)
                fail(call->line, "'print_str' expects a single string literal argument");
            check_expr(call->args[0].get());
            return note(e, Type::Void);
        }
        auto it = functions_.find(call->callee);
        if (it == functions_.end())
            fail(call->line, "call to undeclared function '" + call->callee + "'");
        const FuncInfo& info = it->second;
        if (info.params.size() != call->args.size())
            fail(call->line, "function '" + call->callee + "' expects " + plural(info.params.size()) +
                             ", got " + std::to_string(call->args.size()));
        for (auto& arg : call->args) require_value(check_expr(arg.get()), call->line);
        return note(e, info.ret);
    }
    fail(e->line, "unsupported expression");
}