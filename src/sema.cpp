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
const char* op_text(Tok op) {
    switch (op) {
        case Tok::PLUS: return "+";
        case Tok::MINUS: return "-";
        case Tok::STAR: return "*";
        case Tok::SLASH: return "/";
        case Tok::PERCENT: return "%";
        case Tok::EQ: return "==";
        case Tok::NE: return "!=";
        case Tok::LT: return "<";
        case Tok::GT: return ">";
        case Tok::LE: return "<=";
        case Tok::GE: return ">=";
        case Tok::AND: return "&&";
        case Tok::OR: return "||";
        default: return "?";
    }
}
bool is_null_literal(const Expr* e) {
    const auto* lit = dynamic_cast<const IntLit*>(e);
    return lit != nullptr && lit->value == 0;
}
void require_sized_pointee(Type p, int line) {
    if (p.pointee().is_void()) fail(line, "arithmetic on 'void*' is not allowed");
}
const Type kCharPtr{Base::Char, 1};
}

Sema::Sema() { push_scope(); }

void Sema::push_scope() { scopes_.emplace_back(); }
void Sema::pop_scope() { scopes_.pop_back(); }

void Sema::declare_var(const std::string& name, Type type, int line, bool is_array, long long len) {
    auto& scope = scopes_.back();
    if (scope.count(name)) fail(line, "variable '" + name + "' already declared in this scope");
    scope[name] = VarInfo{type, is_array, len};
}

VarInfo* Sema::lookup_var(const std::string& name, int line) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    fail(line, "undeclared variable '" + name + "'");
}

void Sema::require_value(Type t, int line) {
    if (t.is_void()) fail(line, "void value used in an expression");
}

void Sema::check_object_type(Type t, const char* what, const std::string& name, int line) {
    if (t.is_void()) fail(line, std::string(what) + " '" + name + "' cannot have type void");
}

// Implicit conversion rules.
bool Sema::convertible(Type from, Type to, const Expr* e) const {
    if (from == to) return true;
    if (to.is_ptr()) {
        if (from.is_ptr()) return to.pointee().is_void() || from.pointee().is_void();
        return is_null_literal(e);
    }
    if (from.is_ptr()) return to == ty::Bool;
    return to.is_scalar() && from.is_scalar();
}

void Sema::require_convertible(Type from, Type to, const Expr* e, int line) {
    if (!convertible(from, to, e))
        fail(line, "cannot convert '" + type_name(from) + "' to '" + type_name(to) + "'");
}

void Sema::declare_function(FuncDecl& fn) {
    if (is_builtin_name(fn.name))
        fail(fn.line, "cannot redefine built-in function '" + fn.name + "'");

    FuncInfo info;
    info.ret = type_from_name(fn.return_type);
    for (auto& p : fn.params) {
        Type pt = type_from_name(p.type);
        check_object_type(pt, "parameter", p.name, fn.line);
        info.params.push_back(pt);
    }
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

    FuncInfo int_fn;
    int_fn.ret = ty::Void;
    int_fn.params = {ty::Int};
    int_fn.defined = true;
    functions_["print_int"] = int_fn;
    functions_["print_char"] = int_fn;

    FuncInfo str_fn;
    str_fn.ret = ty::Void;
    str_fn.params = {kCharPtr};
    str_fn.defined = true;
    functions_["print_str"] = str_fn;

    for (auto& fn : program.functions) {
        declare_function(*fn);
        if (!fn->body) continue;

        current_function_ = fn->name;
        current_ret_ = type_from_name(fn->return_type);
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
    if (auto* av = dynamic_cast<ArrayVarDecl*>(s)) {
        Type elem = type_from_name(av->type);
        check_object_type(elem, "array", av->name, av->line);
        if (av->length < 1 || av->length > 65536)
            fail(av->line, "array size must be between 1 and 65536");
        declare_var(av->name, elem.pointer_to(), av->line, true, av->length);
    } else if (auto* v = dynamic_cast<VarDecl*>(s)) {
        Type t = type_from_name(v->type);
        check_object_type(t, "variable", v->name, v->line);
        if (v->init) {
            Type it = check_expr(v->init.get());
            require_value(it, v->line);
            require_convertible(it, t, v->init.get(), v->line);
        }
        declare_var(v->name, t, v->line);
    } else if (auto* es = dynamic_cast<ExprStmt*>(s)) {
        check_expr(es->expr.get());
    } else if (auto* r = dynamic_cast<Return*>(s)) {
        if (r->value) {
            Type t = check_expr(r->value.get());
            if (current_ret_.is_void()) fail(r->line, "void function should not return a value");
            require_value(t, r->line);
            require_convertible(t, current_ret_, r->value.get(), r->line);
        } else if (!current_ret_.is_void()) {
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

Type Sema::check_binary(Binary* b) {
    Type l = check_expr(b->lhs.get());
    Type r = check_expr(b->rhs.get());
    require_value(l, b->line);
    require_value(r, b->line);
    const std::string bad = std::string("invalid operands to binary '") + op_text(b->op) + "'";
    const bool dbl = l.is_double() || r.is_double();

    switch (b->op) {
        case Tok::AND: case Tok::OR:
            return ty::Bool;
        case Tok::PLUS:
            if (l.is_ptr() && r.is_integral()) { require_sized_pointee(l, b->line); return l; }
            if (l.is_integral() && r.is_ptr()) { require_sized_pointee(r, b->line); return r; }
            if (l.is_ptr() || r.is_ptr()) fail(b->line, bad);
            return dbl ? ty::Double : ty::Int;
        case Tok::MINUS:
            if (l.is_ptr() && r.is_integral()) { require_sized_pointee(l, b->line); return l; }
            if (l.is_ptr() && r.is_ptr()) {
                if (l != r) fail(b->line, bad);
                require_sized_pointee(l, b->line);
                return ty::Int;
            }
            if (l.is_ptr() || r.is_ptr()) fail(b->line, bad);
            return dbl ? ty::Double : ty::Int;
        case Tok::STAR: case Tok::SLASH:
            if (l.is_ptr() || r.is_ptr()) fail(b->line, bad);
            return dbl ? ty::Double : ty::Int;
        case Tok::PERCENT:
            if (l.is_ptr() || r.is_ptr()) fail(b->line, bad);
            if (dbl) fail(b->line, "operator '%' requires integer operands");
            return ty::Int;
        case Tok::EQ: case Tok::NE: case Tok::LT: case Tok::GT: case Tok::LE: case Tok::GE:
            if (l.is_ptr() || r.is_ptr()) {
                bool ok = false;
                if (l.is_ptr() && r.is_ptr())
                    ok = (l == r) || l.pointee().is_void() || r.pointee().is_void();
                else
                    ok = is_null_literal(l.is_ptr() ? b->rhs.get() : b->lhs.get());
                if (!ok) fail(b->line, bad);
            }
            return ty::Bool;
        default:
            fail(b->line, "unsupported binary operator");
    }
}

Type Sema::check_expr(Expr* e) {
    if (dynamic_cast<IntLit*>(e)) return note(e, ty::Int);
    if (dynamic_cast<CharLit*>(e)) return note(e, ty::Char);
    if (dynamic_cast<BoolLit*>(e)) return note(e, ty::Bool);
    if (dynamic_cast<DoubleLit*>(e)) return note(e, ty::Double);
    if (dynamic_cast<StringLit*>(e)) return note(e, kCharPtr);

    if (auto* v = dynamic_cast<VarRef*>(e)) {
        return note(e, lookup_var(v->name, v->line)->type);
    }
    if (auto* c = dynamic_cast<CastExpr*>(e)) {
        Type from = check_expr(c->operand.get());
        require_value(from, c->line);
        Type to = type_from_name(c->type);
        if (to.is_void()) fail(c->line, "cannot cast to 'void'");
        bool ok = true;
        if (to.is_ptr()) ok = from.is_ptr() || from.is_integral();
        else if (from.is_ptr()) ok = to.is_integral();
        if (!ok) fail(c->line, "invalid cast from '" + type_name(from) + "' to '" + type_name(to) + "'");
        return note(e, to);
    }
    if (auto* ad = dynamic_cast<AddrOfExpr*>(e)) {
        Expr* op = ad->operand.get();
        Type t = check_expr(op);
        if (auto* vr = dynamic_cast<VarRef*>(op)) {
            if (lookup_var(vr->name, vr->line)->is_array)
                fail(ad->line, "cannot take the address of an array");
        } else if (!dynamic_cast<IndexExpr*>(op) && !dynamic_cast<DerefExpr*>(op)) {
            fail(ad->line, "operand of '&' must be an lvalue");
        }
        require_value(t, ad->line);
        return note(e, t.pointer_to());
    }
    if (auto* d = dynamic_cast<DerefExpr*>(e)) {
        Type t = check_expr(d->operand.get());
        if (!t.is_ptr()) fail(d->line, "cannot dereference non-pointer type '" + type_name(t) + "'");
        if (t.pointee().is_void()) fail(d->line, "cannot dereference '" + type_name(t) + "'");
        return note(e, t.pointee());
    }
    if (auto* ix = dynamic_cast<IndexExpr*>(e)) {
        Type bt = check_expr(ix->base.get());
        Type it = check_expr(ix->index.get());
        if (!bt.is_ptr()) fail(ix->line, "subscripted value is not a pointer or array");
        if (!it.is_integral()) fail(ix->line, "array index must be an integer");
        if (bt.pointee().is_void()) fail(ix->line, "cannot dereference '" + type_name(bt) + "'");
        return note(e, bt.pointee());
    }
    if (auto* u = dynamic_cast<Unary*>(e)) {
        Type t = check_expr(u->operand.get());
        require_value(t, u->line);
        switch (u->op) {
            case Tok::MINUS:
                if (!t.is_scalar()) fail(u->line, "operator '-' requires a numeric operand");
                return note(e, t.is_double() ? ty::Double : ty::Int);
            case Tok::NOT:
                return note(e, ty::Bool);
            case Tok::TILDE:
                if (!t.is_integral()) fail(u->line, "operator '~' requires an integer operand");
                return note(e, ty::Int);
            default:
                fail(u->line, "unsupported unary operator");
        }
    }
    if (auto* b = dynamic_cast<Binary*>(e)) {
        return note(e, check_binary(b));
    }
    if (auto* a = dynamic_cast<Assign*>(e)) {
        VarInfo* vi = lookup_var(a->name, a->line);
        if (vi->is_array) fail(a->line, "cannot assign to array '" + a->name + "'");
        Type target = vi->type;
        Type vt = check_expr(a->value.get());
        require_value(vt, a->line);
        require_convertible(vt, target, a->value.get(), a->line);
        return note(e, target);
    }
    if (auto* st = dynamic_cast<StoreExpr*>(e)) {
        Type tt = check_expr(st->target.get());
        Type vt = check_expr(st->value.get());
        require_value(vt, st->line);
        require_convertible(vt, tt, st->value.get(), st->line);
        return note(e, tt);
    }
    if (auto* call = dynamic_cast<Call*>(e)) {
        auto it = functions_.find(call->callee);
        if (it == functions_.end())
            fail(call->line, "call to undeclared function '" + call->callee + "'");
        const FuncInfo& info = it->second;
        if (info.params.size() != call->args.size())
            fail(call->line, "function '" + call->callee + "' expects " + plural(info.params.size()) +
                             ", got " + std::to_string(call->args.size()));
        for (size_t i = 0; i < call->args.size(); ++i) {
            Type at = check_expr(call->args[i].get());
            require_value(at, call->line);
            require_convertible(at, info.params[i], call->args[i].get(), call->line);
        }
        return note(e, info.ret);
    }
    fail(e->line, "unsupported expression");
}