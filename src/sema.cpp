#include "sema.h"
#include "ast_ext.h"
#include <set>
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
        case Tok::AMP: return "&";
        case Tok::PIPE: return "|";
        case Tok::CARET: return "^";
        case Tok::SHL: return "<<";
        case Tok::SHR: return ">>";
        default: return "?";
    }
}
bool is_null_literal(const Expr* e) {
    const auto* lit = dynamic_cast<const IntLit*>(e);
    return lit != nullptr && lit->value == 0;
}
void require_sized_pointee(Type p, int line) {
    Type e = p.pointee();
    if (e.is_void()) fail(line, "arithmetic on 'void*' is not allowed");
    if (e.is_struct() && !struct_defs()[static_cast<size_t>(e.sid)].complete)
        fail(line, "arithmetic on pointer to incomplete type '" + type_name(e) + "'");
}
const Type kCharPtr{Base::Char, 1, -1};

Type binary_type(Tok op, Type l, Type r, const Expr* le, const Expr* re, int line) {
    const std::string bad = std::string("invalid operands to binary '") + op_text(op) + "'";
    const bool dbl = l.is_double() || r.is_double();

    switch (op) {
        case Tok::AND: case Tok::OR:
            return ty::Bool;
        case Tok::PLUS:
            if (l.is_ptr() && r.is_integral()) { require_sized_pointee(l, line); return l; }
            if (l.is_integral() && r.is_ptr()) { require_sized_pointee(r, line); return r; }
            if (l.is_ptr() || r.is_ptr()) fail(line, bad);
            return dbl ? ty::Double : ty::Int;
        case Tok::MINUS:
            if (l.is_ptr() && r.is_integral()) { require_sized_pointee(l, line); return l; }
            if (l.is_ptr() && r.is_ptr()) {
                if (l != r) fail(line, bad);
                require_sized_pointee(l, line);
                return ty::Int;
            }
            if (l.is_ptr() || r.is_ptr()) fail(line, bad);
            return dbl ? ty::Double : ty::Int;
        case Tok::STAR: case Tok::SLASH:
            if (l.is_ptr() || r.is_ptr()) fail(line, bad);
            return dbl ? ty::Double : ty::Int;
        case Tok::PERCENT:
            if (l.is_ptr() || r.is_ptr()) fail(line, bad);
            if (dbl) fail(line, "operator '%' requires integer operands");
            return ty::Int;
        case Tok::AMP: case Tok::PIPE: case Tok::CARET: case Tok::SHL: case Tok::SHR:
            if (!l.is_integral() || !r.is_integral())
                fail(line, std::string("operator '") + op_text(op) + "' requires integer operands");
            return ty::Int;
        case Tok::EQ: case Tok::NE: case Tok::LT: case Tok::GT: case Tok::LE: case Tok::GE:
            if (l.is_ptr() || r.is_ptr()) {
                bool ok = false;
                if (l.is_ptr() && r.is_ptr())
                    ok = (l == r) || l.pointee().is_void() || r.pointee().is_void();
                else
                    ok = is_null_literal(l.is_ptr() ? re : le);
                if (!ok) fail(line, bad);
            }
            return ty::Bool;
        default:
            fail(line, "unsupported binary operator");
    }
}

Type cond_type(Type a, Type b, const Expr* ae, const Expr* be, int line) {
    if (a == b) return a;
    if (a.is_scalar() && b.is_scalar()) return (a.is_double() || b.is_double()) ? ty::Double : ty::Int;
    if (a.is_ptr() && b.is_ptr()) {
        if (a.pointee().is_void()) return a;
        if (b.pointee().is_void()) return b;
        fail(line, "incompatible types in conditional expression");
    }
    if (a.is_ptr() && is_null_literal(be)) return a;
    if (b.is_ptr() && is_null_literal(ae)) return b;
    fail(line, "incompatible types in conditional expression");
}
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
    auto g = globals_.find(name);
    if (g != globals_.end()) return &g->second;
    fail(line, "undeclared variable '" + name + "'");
}

void Sema::require_value(Type t, int line) {
    if (t.is_void()) fail(line, "void value used in an expression");
    if (t.is_struct())
        fail(line, "struct values cannot be copied, passed or returned; use a pointer");
}

// Like require_value, but struct values are allowed (copy, pass, return).
void Sema::require_rvalue(Type t, int line) {
    if (t.is_void()) fail(line, "void value used in an expression");
}

void Sema::check_object_type(Type t, const char* what, const std::string& name, int line) {
    if (t.is_void()) fail(line, std::string(what) + " '" + name + "' cannot have type void");
    if (t.is_struct() && !struct_defs()[static_cast<size_t>(t.sid)].complete)
        fail(line, std::string(what) + " '" + name + "' has incomplete type '" + type_name(t) + "'");
}

// Implicit conversion rules.
bool Sema::convertible(Type from, Type to, const Expr* e) const {
    if (from == to) return true;
    if (from.is_struct() || to.is_struct()) return false;
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

void Sema::check_modifiable(Expr* target) {
    if (auto* vr = dynamic_cast<VarRef*>(target)) {
        if (lookup_var(vr->name, vr->line)->is_array)
            fail(vr->line, "cannot modify array '" + vr->name + "'");
    }
}

void Sema::declare_function(FuncDecl& fn) {
    if (is_builtin_name(fn.name))
        fail(fn.line, "cannot redefine built-in function '" + fn.name + "'");
    if (globals_.count(fn.name))
        fail(fn.line, "'" + fn.name + "' redeclared as a different kind of symbol");

    FuncInfo info;
    info.ret = type_from_name(fn.return_type);
    if (info.ret.is_struct() && !struct_defs()[static_cast<size_t>(info.ret.sid)].complete)
        fail(fn.line, "function '" + fn.name + "' returns incomplete type '" + type_name(info.ret) + "'");
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

bool Sema::is_const_init(const Expr* e) const {
    if (dynamic_cast<const IntLit*>(e) || dynamic_cast<const CharLit*>(e) ||
        dynamic_cast<const BoolLit*>(e) || dynamic_cast<const DoubleLit*>(e) ||
        dynamic_cast<const StringLit*>(e))
        return true;
    if (const auto* u = dynamic_cast<const Unary*>(e)) {
        if (u->op != Tok::MINUS) return false;
        const Expr* o = u->operand.get();
        return dynamic_cast<const IntLit*>(o) || dynamic_cast<const CharLit*>(o) ||
               dynamic_cast<const DoubleLit*>(o);
    }
    return false;
}

void Sema::check_array_init(ArrayInit& ai, Type elem, long long& len, int line, const std::string& name) {
    const bool inferred = (len == -1);
    if (!ai.has_list && !ai.has_str) {
        if (inferred) fail(line, "array '" + name + "' needs a size or an initializer");
        return;
    }
    if (elem.is_struct()) fail(line, "arrays of struct cannot have initializers");
    if (ai.has_str) {
        if (elem != ty::Char) fail(line, "string initializer requires a char array");
        long long need = static_cast<long long>(ai.str.size()) + 1;
        if (inferred) len = need;
        else if (need > len) fail(line, "initializer-string is too long for array '" + name + "'");
    } else {
        long long n = static_cast<long long>(ai.list.size());
        if (inferred) {
            if (n == 0) fail(line, "array '" + name + "' needs a size or an initializer");
            len = n;
        } else if (n > len) {
            fail(line, "too many initializers for array '" + name + "'");
        }
        for (auto& e : ai.list) {
            Type t = check_expr(e.get());
            require_value(t, line);
            require_convertible(t, elem, e.get(), line);
        }
    }
}

void Sema::check_global(GlobalDecl& g) {
    Type t = type_from_name(g.type);
    check_object_type(t, "variable", g.name, g.line);
    if (globals_.count(g.name)) fail(g.line, "redefinition of global '" + g.name + "'");

    if (g.is_array) {
        if (g.length != -1 && (g.length < 1 || g.length > 65536))
            fail(g.line, "array size must be between 1 and 65536");
        for (auto& e : g.init_data.list)
            if (!is_const_init(e.get())) fail(g.line, "global initializer must be a constant");
        check_array_init(g.init_data, t, g.length, g.line, g.name);
        if (g.length < 1 || g.length > 65536)
            fail(g.line, "array size must be between 1 and 65536");
        globals_[g.name] = VarInfo{t.pointer_to(), true, g.length};
        return;
    }

    if (g.init) {
        if (t.is_struct()) fail(g.line, "struct initializers are not supported");
        if (!is_const_init(g.init.get())) fail(g.line, "global initializer must be a constant");
        Type it = check_expr(g.init.get());
        require_value(it, g.line);
        require_convertible(it, t, g.init.get(), g.line);
    }
    globals_[g.name] = VarInfo{t, false, 0};
}

void Sema::analyze(Program& program, Globals& globals) {
    functions_.clear();
    types_.clear();
    globals_.clear();

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

    for (auto& g : globals) check_global(*g);

    for (auto& fn : program.functions) {
        declare_function(*fn);
        if (!fn->body) continue;

        current_function_ = fn->name;
        current_ret_ = type_from_name(fn->return_type);
        loop_depth_ = 0;
        switch_depth_ = 0;
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
        if (av->length != -1 && (av->length < 1 || av->length > 65536))
            fail(av->line, "array size must be between 1 and 65536");
        check_array_init(av->init_data, elem, av->length, av->line, av->name);
        if (av->length < 1 || av->length > 65536)
            fail(av->line, "array size must be between 1 and 65536");
        declare_var(av->name, elem.pointer_to(), av->line, true, av->length);
    } else if (auto* v = dynamic_cast<VarDecl*>(s)) {
        Type t = type_from_name(v->type);
        check_object_type(t, "variable", v->name, v->line);
        if (v->init) {
            Type it = check_expr(v->init.get());
            require_rvalue(it, v->line);
            require_convertible(it, t, v->init.get(), v->line);
        }
        declare_var(v->name, t, v->line);
    } else if (auto* es = dynamic_cast<ExprStmt*>(s)) {
        check_expr(es->expr.get());
    } else if (auto* r = dynamic_cast<Return*>(s)) {
        if (r->value) {
            Type t = check_expr(r->value.get());
            if (current_ret_.is_void()) fail(r->line, "void function should not return a value");
            require_rvalue(t, r->line);
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
    } else if (auto* dw = dynamic_cast<DoWhile*>(s)) {
        loop_depth_++;
        check_block(dw->body.get());
        loop_depth_--;
        require_value(check_expr(dw->cond.get()), dw->line);
    } else if (auto* f = dynamic_cast<For*>(s)) {
        push_scope();
        if (f->init) check_stmt(f->init.get());
        if (f->cond) require_value(check_expr(f->cond.get()), f->line);
        if (f->step) check_expr(f->step.get());
        loop_depth_++;
        check_block(f->body.get());
        loop_depth_--;
        pop_scope();
    } else if (auto* sw = dynamic_cast<Switch*>(s)) {
        Type ct = check_expr(sw->cond.get());
        require_value(ct, sw->line);
        if (!ct.is_integral()) fail(sw->line, "switch condition must be an integer");
        std::set<long long> seen;
        bool has_default = false;
        switch_depth_++;
        push_scope();
        for (auto& st : sw->body->stmts) {
            if (auto* cl = dynamic_cast<CaseLabel*>(st.get())) {
                if (cl->is_default) {
                    if (has_default) fail(cl->line, "multiple 'default' labels in one switch");
                    has_default = true;
                } else if (!seen.insert(cl->value).second) {
                    fail(cl->line, "duplicate case value " + std::to_string(cl->value));
                }
                continue;
            }
            check_stmt(st.get());
        }
        pop_scope();
        switch_depth_--;
    } else if (dynamic_cast<Break*>(s) != nullptr) {
        if (loop_depth_ == 0 && switch_depth_ == 0) fail(s->line, "'break' outside of loop or switch");
    } else if (dynamic_cast<Continue*>(s) != nullptr) {
        if (loop_depth_ == 0) fail(s->line, "'continue' outside of loop");
    } else if (auto* blk = dynamic_cast<Block*>(s)) {
        check_block(blk);
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
        if (to.is_struct()) fail(c->line, "cannot cast to a struct type");
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
        } else if (!dynamic_cast<IndexExpr*>(op) && !dynamic_cast<DerefExpr*>(op) &&
                   !dynamic_cast<MemberExpr*>(op)) {
            fail(ad->line, "operand of '&' must be an lvalue");
        }
        if (t.is_void()) fail(ad->line, "void value used in an expression");
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
        Type elem = bt.pointee();
        if (elem.is_void()) fail(ix->line, "cannot dereference '" + type_name(bt) + "'");
        if (elem.is_struct() && !struct_defs()[static_cast<size_t>(elem.sid)].complete)
            fail(ix->line, "indexing pointer to incomplete type '" + type_name(elem) + "'");
        return note(e, elem);
    }
    if (auto* m = dynamic_cast<MemberExpr*>(e)) {
        Type bt = check_expr(m->base.get());
        Type st;
        if (m->arrow) {
            if (!(bt.ptr == 1 && bt.base == Base::Struct))
                fail(m->line, "'->' requires a pointer to struct, got '" + type_name(bt) + "'");
            st = bt.pointee();
        } else {
            if (!bt.is_struct())
                fail(m->line, "'.' requires a struct value, got '" + type_name(bt) + "'");
            st = bt;
        }
        const StructDef& def = struct_defs()[static_cast<size_t>(st.sid)];
        if (!def.complete) fail(m->line, "member access into incomplete type '" + type_name(st) + "'");
        const StructField* f = find_field(st.sid, m->name);
        if (!f) fail(m->line, "struct '" + def.name + "' has no member '" + m->name + "'");
        return note(e, f->type);
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
        Type l = check_expr(b->lhs.get());
        Type r = check_expr(b->rhs.get());
        require_value(l, b->line);
        require_value(r, b->line);
        return note(e, binary_type(b->op, l, r, b->lhs.get(), b->rhs.get(), b->line));
    }
    if (auto* cx = dynamic_cast<CondExpr*>(e)) {
        require_value(check_expr(cx->cond.get()), cx->line);
        Type a = check_expr(cx->then_e.get());
        Type b = check_expr(cx->else_e.get());
        require_rvalue(a, cx->line);
        require_rvalue(b, cx->line);
        return note(e, cond_type(a, b, cx->then_e.get(), cx->else_e.get(), cx->line));
    }
    if (auto* a = dynamic_cast<Assign*>(e)) {
        VarInfo* vi = lookup_var(a->name, a->line);
        if (vi->is_array) fail(a->line, "cannot assign to array '" + a->name + "'");
        Type target = vi->type;
        Type vt = check_expr(a->value.get());
        require_rvalue(vt, a->line);
        require_convertible(vt, target, a->value.get(), a->line);
        return note(e, target);
    }
    if (auto* st = dynamic_cast<StoreExpr*>(e)) {
        Type tt = check_expr(st->target.get());
        Type vt = check_expr(st->value.get());
        require_rvalue(vt, st->line);
        require_convertible(vt, tt, st->value.get(), st->line);
        return note(e, tt);
    }
    if (auto* cp = dynamic_cast<CompoundExpr*>(e)) {
        Type tt = check_expr(cp->target.get());
        check_modifiable(cp->target.get());
        if (tt.is_struct())
            fail(cp->line, "struct values cannot be copied, passed or returned; use a pointer");
        Type vt = check_expr(cp->value.get());
        require_value(vt, cp->line);
        Type rt = binary_type(cp->op, tt, vt, cp->target.get(), cp->value.get(), cp->line);
        require_convertible(rt, tt, nullptr, cp->line);
        return note(e, tt);
    }
    if (auto* id = dynamic_cast<IncDecExpr*>(e)) {
        Type t = check_expr(id->target.get());
        check_modifiable(id->target.get());
        bool ok = (t.is_integral() && t != ty::Bool) || t.is_double() ||
                  (t.is_ptr() && !t.pointee().is_void());
        if (!ok) fail(id->line, "invalid operand type '" + type_name(t) + "' for '++'/'--'");
        if (t.is_ptr()) require_sized_pointee(t, id->line);
        return note(e, t);
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
            require_rvalue(at, call->line);
            require_convertible(at, info.params[i], call->args[i].get(), call->line);
        }
        return note(e, info.ret);
    }
    fail(e->line, "unsupported expression");
}
