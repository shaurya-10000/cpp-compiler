#include "optimizer.h"

#include <climits>
#include <stdexcept>
#include <utility>

namespace {

bool const_int_value(const Expr* e, long long& v) {
    if (const auto* n = dynamic_cast<const IntLit*>(e)) {
        v = n->value;
        return true;
    }
    if (const auto* c = dynamic_cast<const CharLit*>(e)) {
        v = static_cast<signed char>(c->value);
        return true;
    }
    if (const auto* b = dynamic_cast<const BoolLit*>(e)) {
        v = b->value ? 1 : 0;
        return true;
    }
    return false;
}

// Expressions that cannot trap and have no side effects (so they may be dropped).
bool is_pure(const Expr* e) {
    if (dynamic_cast<const IntLit*>(e) || dynamic_cast<const CharLit*>(e) ||
        dynamic_cast<const BoolLit*>(e) || dynamic_cast<const DoubleLit*>(e) ||
        dynamic_cast<const StringLit*>(e) || dynamic_cast<const VarRef*>(e))
        return true;
    if (const auto* u = dynamic_cast<const Unary*>(e)) return is_pure(u->operand.get());
    if (const auto* c = dynamic_cast<const CastExpr*>(e)) return is_pure(c->operand.get());
    if (const auto* b = dynamic_cast<const Binary*>(e)) {
        if (b->op == Tok::SLASH || b->op == Tok::PERCENT) return false;   // may trap
        return is_pure(b->lhs.get()) && is_pure(b->rhs.get());
    }
    return false;
}

bool is_commutative(Tok op) {
    return op == Tok::PLUS || op == Tok::STAR || op == Tok::AMP || op == Tok::PIPE || op == Tok::CARET;
}

bool flip_cmp(Tok op, Tok& flipped) {
    switch (op) {
        case Tok::LT: flipped = Tok::GT; return true;
        case Tok::GT: flipped = Tok::LT; return true;
        case Tok::LE: flipped = Tok::GE; return true;
        case Tok::GE: flipped = Tok::LE; return true;
        case Tok::EQ: flipped = Tok::EQ; return true;
        case Tok::NE: flipped = Tok::NE; return true;
        default: return false;
    }
}

// Mirrors the runtime semantics of the generated code (64-bit wraparound, arithmetic >>,
// shift counts masked to 6 bits). Division by zero is never folded.
bool fold_binop(Tok op, long long a, long long b, long long& out, bool& is_bool) {
    using U = unsigned long long;
    is_bool = false;
    switch (op) {
        case Tok::PLUS:  out = static_cast<long long>(static_cast<U>(a) + static_cast<U>(b)); return true;
        case Tok::MINUS: out = static_cast<long long>(static_cast<U>(a) - static_cast<U>(b)); return true;
        case Tok::STAR:  out = static_cast<long long>(static_cast<U>(a) * static_cast<U>(b)); return true;
        case Tok::SLASH:
            if (b == 0 || (a == LLONG_MIN && b == -1)) return false;
            out = a / b;
            return true;
        case Tok::PERCENT:
            if (b == 0 || (a == LLONG_MIN && b == -1)) return false;
            out = a % b;
            return true;
        case Tok::AMP:   out = a & b; return true;
        case Tok::PIPE:  out = a | b; return true;
        case Tok::CARET: out = a ^ b; return true;
        case Tok::SHL:
            out = static_cast<long long>(static_cast<U>(a) << (static_cast<U>(b) & 63U));
            return true;
        case Tok::SHR:
            out = a >> static_cast<int>(static_cast<U>(b) & 63U);
            return true;
        case Tok::EQ: is_bool = true; out = (a == b); return true;
        case Tok::NE: is_bool = true; out = (a != b); return true;
        case Tok::LT: is_bool = true; out = (a < b); return true;
        case Tok::GT: is_bool = true; out = (a > b); return true;
        case Tok::LE: is_bool = true; out = (a <= b); return true;
        case Tok::GE: is_bool = true; out = (a >= b); return true;
        default: return false;
    }
}

// True when control never continues past this statement.
bool terminates(const Stmt* s) {
    if (dynamic_cast<const Return*>(s) || dynamic_cast<const Break*>(s) ||
        dynamic_cast<const Continue*>(s))
        return true;
    if (const auto* b = dynamic_cast<const Block*>(s)) {
        for (const auto& st : b->stmts)
            if (terminates(st.get())) return true;
        return false;
    }
    if (const auto* i = dynamic_cast<const If*>(s)) {
        return i->else_body != nullptr && terminates(i->then_body.get()) &&
               terminates(i->else_body.get());
    }
    return false;
}

}  // namespace

Type Optimizer::type_of(const Expr* e) const {
    auto it = sema_.expr_types().find(e);
    if (it == sema_.expr_types().end())
        throw std::runtime_error("Optimizer error: expression has no type information");
    return it->second;
}

bool Optimizer::replace_int(ExprPtr& slot, long long v) {
    auto lit = std::make_unique<IntLit>();
    lit->line = slot->line;
    lit->value = v;
    if (static_cast<long long>(lit->value) != v) return false;   // literal type too narrow
    sema_.set_type(lit.get(), ty::Int);
    slot = std::move(lit);
    return true;
}

bool Optimizer::replace_bool(ExprPtr& slot, bool v) {
    auto lit = std::make_unique<BoolLit>();
    lit->line = slot->line;
    lit->value = v;
    sema_.set_type(lit.get(), ty::Bool);
    slot = std::move(lit);
    return true;
}

bool Optimizer::replace_char(ExprPtr& slot, char v) {
    auto lit = std::make_unique<CharLit>();
    lit->line = slot->line;
    lit->value = v;
    sema_.set_type(lit.get(), ty::Char);
    slot = std::move(lit);
    return true;
}

bool Optimizer::replace_double(ExprPtr& slot, double v) {
    auto lit = std::make_unique<DoubleLit>();
    lit->line = slot->line;
    lit->value = v;
    sema_.set_type(lit.get(), ty::Double);
    slot = std::move(lit);
    return true;
}

void Optimizer::fold_binary_node(ExprPtr& e, Binary* b) {
    long long lv = 0, rv = 0;
    bool lc = const_int_value(b->lhs.get(), lv) && type_of(b->lhs.get()).is_integral();
    bool rc = const_int_value(b->rhs.get(), rv) && type_of(b->rhs.get()).is_integral();
    const Tok op = b->op;

    // Short-circuit operators.
    if (op == Tok::AND || op == Tok::OR) {
        if (!lc) return;
        bool result = false;
        if (op == Tok::AND) {
            if (lv == 0) result = false;
            else if (rc) result = (rv != 0);
            else return;
        } else {
            if (lv != 0) result = true;
            else if (rc) result = (rv != 0);
            else return;
        }
        if (replace_bool(e, result)) ++stats_.folded;
        return;
    }

    // Both operands constant.
    if (lc && rc) {
        long long out = 0;
        bool is_bool = false;
        if (!fold_binop(op, lv, rv, out, is_bool)) return;
        const bool ok = is_bool ? replace_bool(e, out != 0) : replace_int(e, out);
        if (ok) ++stats_.folded;
        return;
    }

    // Canonical form: the constant operand goes on the right.
    if (lc && !rc) {
        const Type et = type_of(e.get());
        const Type rt0 = type_of(b->rhs.get());
        Tok flipped = op;
        bool swapped = false;
        if (et == ty::Int && rt0.is_integral() && is_commutative(op)) {
            swapped = true;
        } else if (et == ty::Bool && rt0.is_integral() && flip_cmp(op, flipped)) {
            b->op = flipped;
            swapped = true;
        }
        if (swapped) {
            std::swap(b->lhs, b->rhs);
            std::swap(lv, rv);
            lc = false;
            rc = true;
            ++stats_.simplified;
        }
    }

    // Identities on plain int arithmetic: x op const.
    if (!rc) return;
    if (type_of(e.get()) != ty::Int) return;
    if (type_of(b->lhs.get()) != ty::Int) return;

    const bool id_zero = (rv == 0) && (op == Tok::PLUS || op == Tok::MINUS || op == Tok::PIPE ||
                                       op == Tok::CARET || op == Tok::SHL || op == Tok::SHR);
    const bool id_one = (rv == 1) && (op == Tok::STAR || op == Tok::SLASH);
    if (id_zero || id_one) {
        ExprPtr keep = std::move(b->lhs);
        e = std::move(keep);
        ++stats_.simplified;
        return;
    }
    if (rv == 0 && (op == Tok::STAR || op == Tok::AMP) && is_pure(b->lhs.get())) {
        if (replace_int(e, 0)) ++stats_.simplified;
        return;
    }
}

void Optimizer::fold(ExprPtr& e) {
    if (!e) return;
    Expr* p = e.get();

    if (auto* u = dynamic_cast<Unary*>(p)) {
        fold(u->operand);
        long long v = 0;
        if (const_int_value(u->operand.get(), v) && type_of(u->operand.get()).is_integral()) {
            bool ok = false;
            switch (u->op) {
                case Tok::MINUS:
                    ok = replace_int(e, static_cast<long long>(0ULL - static_cast<unsigned long long>(v)));
                    break;
                case Tok::TILDE: ok = replace_int(e, ~v); break;
                case Tok::NOT:   ok = replace_bool(e, v == 0); break;
                default: break;
            }
            if (ok) ++stats_.folded;
        }
        return;
    }
    if (auto* b = dynamic_cast<Binary*>(p)) {
        fold(b->lhs);
        fold(b->rhs);
        fold_binary_node(e, b);
        return;
    }
    if (auto* c = dynamic_cast<CastExpr*>(p)) {
        fold(c->operand);
        long long v = 0;
        const Type to = type_of(e.get());
        if (const_int_value(c->operand.get(), v) && type_of(c->operand.get()).is_integral()) {
            bool ok = false;
            if (to == ty::Int) ok = replace_int(e, v);
            else if (to == ty::Bool) ok = replace_bool(e, v != 0);
            else if (to == ty::Char) ok = replace_char(e, static_cast<char>(v));
            else if (to.is_double()) ok = replace_double(e, static_cast<double>(v));
            if (ok) ++stats_.folded;
        }
        return;
    }
    if (auto* cx = dynamic_cast<CondExpr*>(p)) {
        fold(cx->cond);
        fold(cx->then_e);
        fold(cx->else_e);
        long long v = 0;
        if (const_int_value(cx->cond.get(), v) && type_of(cx->cond.get()).is_integral()) {
            ExprPtr& chosen = (v != 0) ? cx->then_e : cx->else_e;
            if (type_of(chosen.get()) == type_of(e.get())) {
                ExprPtr keep = std::move(chosen);
                e = std::move(keep);
                ++stats_.folded;
            }
        }
        return;
    }
    if (auto* a = dynamic_cast<Assign*>(p)) { fold(a->value); return; }
    if (auto* call = dynamic_cast<Call*>(p)) {
        for (auto& arg : call->args) fold(arg);
        return;
    }
    if (auto* ad = dynamic_cast<AddrOfExpr*>(p)) { fold(ad->operand); return; }
    if (auto* d = dynamic_cast<DerefExpr*>(p)) { fold(d->operand); return; }
    if (auto* ix = dynamic_cast<IndexExpr*>(p)) { fold(ix->base); fold(ix->index); return; }
    if (auto* m = dynamic_cast<MemberExpr*>(p)) { fold(m->base); return; }
    if (auto* st = dynamic_cast<StoreExpr*>(p)) { fold(st->target); fold(st->value); return; }
    if (auto* cp = dynamic_cast<CompoundExpr*>(p)) { fold(cp->target); fold(cp->value); return; }
    if (auto* id = dynamic_cast<IncDecExpr*>(p)) { fold(id->target); return; }
}

void Optimizer::opt_stmt(StmtPtr& s) {
    Stmt* p = s.get();

    if (auto* av = dynamic_cast<ArrayVarDecl*>(p)) {
        for (auto& ex : av->init_data.list) fold(ex);
        return;
    }
    if (auto* v = dynamic_cast<VarDecl*>(p)) {
        if (v->init) fold(v->init);
        return;
    }
    if (auto* es = dynamic_cast<ExprStmt*>(p)) {
        fold(es->expr);
        if (is_pure(es->expr.get())) {
            s.reset();
            ++stats_.dead_stmts;
        }
        return;
    }
    if (auto* r = dynamic_cast<Return*>(p)) {
        if (r->value) fold(r->value);
        return;
    }
    if (auto* i = dynamic_cast<If*>(p)) {
        fold(i->cond);
        opt_block(i->then_body.get());
        if (i->else_body) opt_block(i->else_body.get());
        long long v = 0;
        if (const_int_value(i->cond.get(), v) && type_of(i->cond.get()).is_integral()) {
            ++stats_.branches;
            if (v != 0) {
                StmtPtr taken = std::move(i->then_body);
                s = std::move(taken);
            } else if (i->else_body) {
                StmtPtr taken = std::move(i->else_body);
                s = std::move(taken);
            } else {
                s.reset();
            }
        }
        return;
    }
    if (auto* w = dynamic_cast<While*>(p)) {
        fold(w->cond);
        opt_block(w->body.get());
        long long v = 0;
        if (const_int_value(w->cond.get(), v) && type_of(w->cond.get()).is_integral() && v == 0) {
            ++stats_.branches;
            s.reset();
        }
        return;
    }
    if (auto* dw = dynamic_cast<DoWhile*>(p)) {
        opt_block(dw->body.get());
        fold(dw->cond);
        return;
    }
    if (auto* f = dynamic_cast<For*>(p)) {
        if (f->init) opt_stmt(f->init);
        if (f->cond) {
            fold(f->cond);
            long long v = 0;
            if (const_int_value(f->cond.get(), v) && type_of(f->cond.get()).is_integral()) {
                ++stats_.branches;
                if (v == 0) {
                    // Loop never runs; only the initializer still executes.
                    auto blk = std::make_unique<Block>();
                    blk->line = f->line;
                    if (f->init) blk->stmts.push_back(std::move(f->init));
                    s = std::move(blk);
                    return;
                }
                f->cond.reset();   // always true: drop the test
            }
        }
        if (f->step) fold(f->step);
        opt_block(f->body.get());
        return;
    }
    if (auto* blk = dynamic_cast<Block*>(p)) {
        opt_block(blk);
        return;
    }
}

void Optimizer::opt_block(Block* b) {
    std::vector<StmtPtr> kept;
    const size_t n = b->stmts.size();
    kept.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        StmtPtr& s = b->stmts[i];
        opt_stmt(s);
        if (!s) continue;
        const bool stop = terminates(s.get());
        kept.push_back(std::move(s));
        if (stop) {
            stats_.dead_stmts += static_cast<int>(n - i - 1);
            break;
        }
    }
    b->stmts = std::move(kept);
}

void Optimizer::run(Program& program) {
    for (auto& fn : program.functions)
        if (fn->body) opt_block(fn->body.get());
}