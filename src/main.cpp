#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <iostream>
#include <sstream>

static void pad(int n) { for (int i = 0; i < n; ++i) std::cout << "  "; }
static void dump_expr(const Expr* e, int ind);
static void dump_stmt(const Stmt* s, int ind);

static void dump_block(const Block* b, int ind) {
    for (auto& s : b->stmts) dump_stmt(s.get(), ind);
}

static void dump_stmt(const Stmt* s, int ind) {
    pad(ind);
    if (auto* d = dynamic_cast<const VarDecl*>(s)) {
        std::cout << "VarDecl " << d->type << " " << d->name << "\n";
        if (d->init) dump_expr(d->init.get(), ind+1);
    } else if (auto* r = dynamic_cast<const Return*>(s)) {
        std::cout << "Return\n";
        if (r->value) dump_expr(r->value.get(), ind+1);
    } else if (auto* i = dynamic_cast<const If*>(s)) {
        std::cout << "If\n";
        pad(ind+1); std::cout << "cond:\n"; dump_expr(i->cond.get(), ind+2);
        pad(ind+1); std::cout << "then:\n"; dump_block(i->then_body.get(), ind+2);
        if (i->else_body) { pad(ind+1); std::cout << "else:\n"; dump_block(i->else_body.get(), ind+2); }
    } else if (auto* w = dynamic_cast<const While*>(s)) {
        std::cout << "While\n";
        pad(ind+1); std::cout << "cond:\n"; dump_expr(w->cond.get(), ind+2);
        pad(ind+1); std::cout << "body:\n"; dump_block(w->body.get(), ind+2);
    } else if (auto* es = dynamic_cast<const ExprStmt*>(s)) {
        std::cout << "ExprStmt\n"; dump_expr(es->expr.get(), ind+1);
    } else if (auto* b = dynamic_cast<const Block*>(s)) {
        std::cout << "Block\n"; dump_block(b, ind+1);
    } else {
        std::cout << "UnknownStmt\n";
    }
}

static void dump_expr(const Expr* e, int ind) {
    pad(ind);
    if (auto* n = dynamic_cast<const IntLit*>(e))       std::cout << "Int " << n->value << "\n";
    else if (auto* c = dynamic_cast<const CharLit*>(e)) std::cout << "Char '" << c->value << "'\n";
    else if (auto* b = dynamic_cast<const BoolLit*>(e)) std::cout << "Bool " << (b->value ? "true" : "false") << "\n";
    else if (auto* v = dynamic_cast<const VarRef*>(e))  std::cout << "Var " << v->name << "\n";
    else if (auto* b = dynamic_cast<const Binary*>(e)) {
        std::cout << "Binary op=" << (int)b->op << "\n";
        dump_expr(b->lhs.get(), ind+1);
        dump_expr(b->rhs.get(), ind+1);
    } else if (auto* a = dynamic_cast<const Assign*>(e)) {
        std::cout << "Assign " << a->name << "\n";
        dump_expr(a->value.get(), ind+1);
    } else if (auto* c = dynamic_cast<const Call*>(e)) {
        std::cout << "Call " << c->callee << "\n";
        for (auto& a : c->args) dump_expr(a.get(), ind+1);
    } else std::cout << "UnknownExpr\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: minicc [--dump-tokens|--dump-ast] <file>\n";
        return 1;
    }
    std::string mode = argv[1];
    std::string filepath = argv[2];

    std::ifstream f(filepath);
    if (!f) { std::cerr << "cannot open " << filepath << "\n"; return 1; }
    std::stringstream ss; ss << f.rdbuf(); std::string src = ss.str();

    try {
        Lexer lex(src);
        auto toks = lex.tokenize();

        if (mode == "--dump-tokens") {
            std::cout << "Tokens: " << toks.size() << "\n";
            for (auto& t : toks) {
                std::cout << "  type=" << (int)t.type << " text='" << t.text
                          << "' line=" << t.line << "\n";
            }
            return 0;
        }

        Parser p(std::move(toks));
        auto prog = p.parse();

        if (mode == "--dump-ast") {
            for (auto& fn : prog.functions) {
                std::cout << "Function " << fn->return_type << " " << fn->name << "(";
                for (size_t i = 0; i < fn->params.size(); ++i) {
                    if (i) std::cout << ", ";
                    std::cout << fn->params[i].type << " " << fn->params[i].name;
                }
                std::cout << ")\n";
                dump_block(fn->body.get(), 1);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
