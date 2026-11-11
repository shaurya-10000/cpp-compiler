#include "ast_dump.h"

#include <string>

namespace {

const char* op_name(Tok op) {
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
        case Tok::NOT: return "!";
        case Tok::TILDE: return "~";
        case Tok::AMP: return "&";
        case Tok::PIPE: return "|";
        case Tok::CARET: return "^";
        case Tok::SHL: return "<<";
        case Tok::SHR: return ">>";
        default: return "?";
    }
}

std::string escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '\n') r += "\\n";
        else if (c == '\t') r += "\\t";
        else if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else r += c;
    }
    return r;
}

class Dumper {
public:
    Dumper(std::ostream& o, const Sema* s) : os(o), sema(s) {}

    void function(const FuncDecl& fn) {
        os << "function " << fn.return_type << " " << fn.name << "(";
        for (size_t i = 0; i < fn.params.size(); ++i) {
            if (i) os << ", ";
            os << fn.params[i].type << " " << fn.params[i].name;
        }
        os << ")";
        if (!fn.body) os << " [prototype]";
        os << "\n";
        if (fn.body) block(fn.body.get(), 1);
    }

    void global(const GlobalDecl& g) {
        os << "global " << g.type << " " << g.name;
        if (g.is_array) os << "[" << g.length << "]";
        os << "\n";
        if (g.init) expr(g.init.get(), 1);
        array_init(g.init_data, 1);
    }

private:
    std::ostream& os;
    const Sema* sema;

    void indent(int d) { for (int i = 0; i < d; ++i) os << "  "; }

    std::string ty(const Expr* e) const {
        if (!sema) return "";
        auto it = sema->expr_types().find(e);
        if (it == sema->expr_types().end()) return "";
        return " : " + type_name(it->second);
    }

    void array_init(const ArrayInit& ai, int d) {
        if (ai.has_str) { indent(d); os << "InitString \"" << escape(ai.str) << "\"\n"; }
        if (ai.has_list) for (const auto& e : ai.list) expr(e.get(), d);
    }

    void block(const Block* b, int d) {
        indent(d);
        os << "Block\n";
        if (!b) return;
        for (const auto& s : b->stmts) stmt(s.get(), d + 1);
    }

    void expr(const Expr* e, int d) {
        indent(d);
        if (e == nullptr) { os << "<null>\n"; return; }

        if (const auto* n = dynamic_cast<const IntLit*>(e)) { os << "IntLit " << n->value << ty(e) << "\n"; return; }
        if (const auto* dl = dynamic_cast<const DoubleLit*>(e)) { os << "DoubleLit " << dl->value << ty(e) << "\n"; return; }
        if (const auto* c = dynamic_cast<const CharLit*>(e)) { os << "CharLit " << static_cast<int>(c->value) << ty(e) << "\n"; return; }
        if (const auto* bl = dynamic_cast<const BoolLit*>(e)) { os << "BoolLit " << (bl->value ? "true" : "false") << ty(e) << "\n"; return; }
        if (const auto* sl = dynamic_cast<const StringLit*>(e)) { os << "StringLit \"" << escape(sl->value) << "\"" << ty(e) << "\n"; return; }
        if (const auto* v = dynamic_cast<const VarRef*>(e)) { os << "VarRef " << v->name << ty(e) << "\n"; return; }
        if (const auto* u = dynamic_cast<const Unary*>(e)) {
            os << "Unary " << op_name(u->op) << ty(e) << "\n";
            expr(u->operand.get(), d + 1);
            return;
        }
        if (const auto* bi = dynamic_cast<const Binary*>(e)) {
            os << "Binary " << op_name(bi->op) << ty(e) << "\n";
            expr(bi->lhs.get(), d + 1);
            expr(bi->rhs.get(), d + 1);
            return;
        }
        if (const auto* a = dynamic_cast<const Assign*>(e)) {
            os << "Assign " << a->name << ty(e) << "\n";
            expr(a->value.get(), d + 1);
            return;
        }
        if (const auto* call = dynamic_cast<const Call*>(e)) {
            os << "Call " << call->callee << ty(e) << "\n";
            for (const auto& arg : call->args) expr(arg.get(), d + 1);
            return;
        }
        if (const auto* cast = dynamic_cast<const CastExpr*>(e)) {
            os << "Cast " << cast->type << ty(e) << "\n";
            expr(cast->operand.get(), d + 1);
            return;
        }
        if (const auto* ad = dynamic_cast<const AddrOfExpr*>(e)) {
            os << "AddrOf" << ty(e) << "\n";
            expr(ad->operand.get(), d + 1);
            return;
        }
        if (const auto* de = dynamic_cast<const DerefExpr*>(e)) {
            os << "Deref" << ty(e) << "\n";
            expr(de->operand.get(), d + 1);
            return;
        }
        if (const auto* ix = dynamic_cast<const IndexExpr*>(e)) {
            os << "Index" << ty(e) << "\n";
            expr(ix->base.get(), d + 1);
            expr(ix->index.get(), d + 1);
            return;
        }
        if (const auto* m = dynamic_cast<const MemberExpr*>(e)) {
            os << "Member " << (m->arrow ? "->" : ".") << m->name << ty(e) << "\n";
            expr(m->base.get(), d + 1);
            return;
        }
        if (const auto* st = dynamic_cast<const StoreExpr*>(e)) {
            os << "Store" << ty(e) << "\n";
            expr(st->target.get(), d + 1);
            expr(st->value.get(), d + 1);
            return;
        }
        if (const auto* cp = dynamic_cast<const CompoundExpr*>(e)) {
            os << "Compound " << op_name(cp->op) << "=" << ty(e) << "\n";
            expr(cp->target.get(), d + 1);
            expr(cp->value.get(), d + 1);
            return;
        }
        if (const auto* id = dynamic_cast<const IncDecExpr*>(e)) {
            os << (id->prefix ? "Pre" : "Post") << (id->inc ? "Inc" : "Dec") << ty(e) << "\n";
            expr(id->target.get(), d + 1);
            return;
        }
        if (const auto* cx = dynamic_cast<const CondExpr*>(e)) {
            os << "Cond" << ty(e) << "\n";
            expr(cx->cond.get(), d + 1);
            expr(cx->then_e.get(), d + 1);
            expr(cx->else_e.get(), d + 1);
            return;
        }
        os << "<unknown expr>\n";
    }

    void stmt(const Stmt* s, int d) {
        if (s == nullptr) { indent(d); os << "<null>\n"; return; }

        if (const auto* av = dynamic_cast<const ArrayVarDecl*>(s)) {
            indent(d);
            os << "ArrayVarDecl " << av->type << " " << av->name << "[" << av->length << "]\n";
            array_init(av->init_data, d + 1);
            return;
        }
        if (const auto* v = dynamic_cast<const VarDecl*>(s)) {
            indent(d);
            os << "VarDecl " << v->type << " " << v->name << "\n";
            if (v->init) expr(v->init.get(), d + 1);
            return;
        }
        if (const auto* es = dynamic_cast<const ExprStmt*>(s)) {
            indent(d);
            os << "ExprStmt\n";
            expr(es->expr.get(), d + 1);
            return;
        }
        if (const auto* r = dynamic_cast<const Return*>(s)) {
            indent(d);
            os << "Return\n";
            if (r->value) expr(r->value.get(), d + 1);
            return;
        }
        if (const auto* i = dynamic_cast<const If*>(s)) {
            indent(d);
            os << "If\n";
            expr(i->cond.get(), d + 1);
            indent(d + 1);
            os << "Then\n";
            block(i->then_body.get(), d + 2);
            if (i->else_body) {
                indent(d + 1);
                os << "Else\n";
                block(i->else_body.get(), d + 2);
            }
            return;
        }
        if (const auto* w = dynamic_cast<const While*>(s)) {
            indent(d);
            os << "While\n";
            expr(w->cond.get(), d + 1);
            block(w->body.get(), d + 1);
            return;
        }
        if (const auto* dw = dynamic_cast<const DoWhile*>(s)) {
            indent(d);
            os << "DoWhile\n";
            block(dw->body.get(), d + 1);
            expr(dw->cond.get(), d + 1);
            return;
        }
        if (const auto* f = dynamic_cast<const For*>(s)) {
            indent(d);
            os << "For\n";
            if (f->init) { indent(d + 1); os << "Init\n"; stmt(f->init.get(), d + 2); }
            if (f->cond) { indent(d + 1); os << "Cond\n"; expr(f->cond.get(), d + 2); }
            if (f->step) { indent(d + 1); os << "Step\n"; expr(f->step.get(), d + 2); }
            indent(d + 1);
            os << "Body\n";
            block(f->body.get(), d + 2);
            return;
        }
        if (dynamic_cast<const Break*>(s)) { indent(d); os << "Break\n"; return; }
        if (dynamic_cast<const Continue*>(s)) { indent(d); os << "Continue\n"; return; }
        if (const auto* blk = dynamic_cast<const Block*>(s)) { block(blk, d); return; }
        indent(d);
        os << "<unknown stmt>\n";
    }
};

}  // namespace

void dump_program(const Program& prog, const Globals& globals, std::ostream& os,
                  const Sema* sema, std::size_t skip_functions) {
    Dumper d(os, sema);
    for (const auto& g : globals) d.global(*g);
    for (std::size_t i = skip_functions; i < prog.functions.size(); ++i) d.function(*prog.functions[i]);
}