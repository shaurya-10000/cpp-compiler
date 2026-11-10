#include "parser.h"
#include "ast_ext.h"
#include "types.h"
#include <stdexcept>

Parser::Parser(std::vector<Token> toks) : toks_(std::move(toks)) {}

const Token& Parser::peek(int off) const {
    size_t i = pos_ + off;
    return i < toks_.size() ? toks_[i] : toks_.back();
}
const Token& Parser::advance() { return toks_[pos_++]; }
bool Parser::check(Tok t) const { return peek().type == t; }
bool Parser::match(Tok t) { if (check(t)) { advance(); return true; } return false; }
const Token& Parser::expect(Tok t, const char* what) {
    if (!check(t)) error(std::string("expected ") + what);
    return advance();
}
void Parser::error(const std::string& msg) const {
    throw std::runtime_error("Parse error at line " + std::to_string(peek().line) +
                             ": " + msg + " (got '" + peek().text + "')");
}

bool Parser::is_type_kw(Tok t) const {
    return t == Tok::KW_INT || t == Tok::KW_BOOL || t == Tok::KW_CHAR ||
           t == Tok::KW_DOUBLE || t == Tok::KW_VOID;
}

// type := (int|bool|char|double|void) '*'*      returns e.g. "char*"
std::string Parser::parse_type() {
    if (!is_type_kw(peek().type)) error("expected type");
    std::string name = advance().text;
    while (match(Tok::STAR)) name += '*';
    return name;
}

// '(' type '*'* ')'
bool Parser::looks_like_cast() const {
    if (!check(Tok::LPAREN) || !is_type_kw(peek(1).type)) return false;
    int k = 2;
    while (peek(k).type == Tok::STAR) k++;
    return peek(k).type == Tok::RPAREN;
}

Program Parser::parse() {
    Program p;
    while (!check(Tok::END)) {
        p.functions.push_back(parse_function());
    }
    return p;
}

std::unique_ptr<FuncDecl> Parser::parse_function() {
    auto fn = std::make_unique<FuncDecl>();
    fn->line = peek().line;

    fn->return_type = parse_type();
    fn->name = expect(Tok::IDENT, "function name").text;

    expect(Tok::LPAREN, "'('");
    if (!check(Tok::RPAREN)) {
        do {
            Param p;
            p.type = parse_type();
            p.name = expect(Tok::IDENT, "parameter name").text;
            fn->params.push_back(p);
        } while (match(Tok::COMMA));
    }
    expect(Tok::RPAREN, "')'");

    if (match(Tok::SEMICOLON)) return fn;   // prototype
    fn->body = parse_block();
    return fn;
}

std::unique_ptr<Block> Parser::parse_block() {
    auto block = std::make_unique<Block>();
    block->line = expect(Tok::LBRACE, "'{'").line;
    while (!check(Tok::RBRACE) && !check(Tok::END)) {
        block->stmts.push_back(parse_stmt());
    }
    expect(Tok::RBRACE, "'}'");
    return block;
}

StmtPtr Parser::parse_stmt() {
    if (is_type_kw(peek().type)) return parse_var_decl();
    if (check(Tok::KW_IF)) return parse_if();
    if (check(Tok::KW_WHILE)) return parse_while();
    if (check(Tok::KW_FOR)) return parse_for();
    if (check(Tok::KW_BREAK)) {
        advance();
        auto b = std::make_unique<Break>();
        b->line = toks_[pos_ - 1].line;
        expect(Tok::SEMICOLON, "';'");
        return b;
    }
    if (check(Tok::KW_CONTINUE)) {
        advance();
        auto c = std::make_unique<Continue>();
        c->line = toks_[pos_ - 1].line;
        expect(Tok::SEMICOLON, "';'");
        return c;
    }
    if (check(Tok::KW_RETURN)) return parse_return();
    if (check(Tok::LBRACE)) {
        auto b = parse_block();
        return b;
    }

    auto s = std::make_unique<ExprStmt>();
    s->line = peek().line;
    s->expr = parse_expr();
    expect(Tok::SEMICOLON, "';'");
    return s;
}

StmtPtr Parser::parse_var_decl() {
    int line = peek().line;
    std::string type = parse_type();
    std::string name = expect(Tok::IDENT, "variable name").text;

    if (match(Tok::LBRACKET)) {
        const Token& len = expect(Tok::INT_LIT, "array size");
        auto a = std::make_unique<ArrayVarDecl>();
        a->line = line;
        a->type = type;
        a->name = name;
        a->length = len.int_val;
        expect(Tok::RBRACKET, "']'");
        if (check(Tok::ASSIGN)) error("array initializers are not supported yet");
        expect(Tok::SEMICOLON, "';'");
        return a;
    }

    auto s = std::make_unique<VarDecl>();
    s->line = line;
    s->type = type;
    s->name = name;
    if (match(Tok::ASSIGN)) {
        s->init = parse_expr();
    }
    expect(Tok::SEMICOLON, "';'");
    return s;
}

StmtPtr Parser::parse_if() {
    auto s = std::make_unique<If>();
    s->line = advance().line;
    expect(Tok::LPAREN, "'('");
    s->cond = parse_expr();
    expect(Tok::RPAREN, "')'");
    s->then_body = parse_block();
    if (match(Tok::KW_ELSE)) {
        if (check(Tok::KW_IF)) {
            auto wrapper = std::make_unique<Block>();
            wrapper->line = peek().line;
            wrapper->stmts.push_back(parse_if());
            s->else_body = std::move(wrapper);
        } else {
            s->else_body = parse_block();
        }
    }
    return s;
}

StmtPtr Parser::parse_while() {
    auto s = std::make_unique<While>();
    s->line = advance().line;
    expect(Tok::LPAREN, "'('");
    s->cond = parse_expr();
    expect(Tok::RPAREN, "')'");
    s->body = parse_block();
    return s;
}

StmtPtr Parser::parse_for() {
    auto s = std::make_unique<For>();
    s->line = advance().line;
    expect(Tok::LPAREN, "'('");

    if (check(Tok::SEMICOLON)) {
        advance();
    } else if (is_type_kw(peek().type)) {
        s->init = parse_var_decl();
    } else {
        auto e = std::make_unique<ExprStmt>();
        e->line = peek().line;
        e->expr = parse_expr();
        expect(Tok::SEMICOLON, "';'");
        s->init = std::move(e);
    }

    if (!check(Tok::SEMICOLON)) {
        s->cond = parse_expr();
    }
    expect(Tok::SEMICOLON, "';'");

    if (!check(Tok::RPAREN)) {
        s->step = parse_expr();
    }
    expect(Tok::RPAREN, "')'");

    s->body = parse_block();
    return s;
}

StmtPtr Parser::parse_return() {
    auto s = std::make_unique<Return>();
    s->line = advance().line;
    if (!check(Tok::SEMICOLON)) {
        s->value = parse_expr();
    }
    expect(Tok::SEMICOLON, "';'");
    return s;
}

// ---- Expressions ----
ExprPtr Parser::parse_expr() { return parse_assign(); }

ExprPtr Parser::parse_assign() {
    auto lhs = parse_or();
    if (match(Tok::ASSIGN)) {
        int line = lhs->line;
        if (auto* vr = dynamic_cast<VarRef*>(lhs.get())) {
            auto a = std::make_unique<Assign>();
            a->line = line;
            a->name = vr->name;
            a->value = parse_assign();   // right assoc
            return a;
        }
        if (dynamic_cast<DerefExpr*>(lhs.get()) || dynamic_cast<IndexExpr*>(lhs.get())) {
            auto st = std::make_unique<StoreExpr>();
            st->line = line;
            st->target = std::move(lhs);
            st->value = parse_assign();
            return st;
        }
        error("invalid assignment target");
    }
    return lhs;
}

ExprPtr Parser::parse_or() {
    auto e = parse_and();
    while (match(Tok::OR)) {
        auto b = std::make_unique<Binary>();
        b->line = e->line; b->op = Tok::OR;
        b->lhs = std::move(e); b->rhs = parse_and();
        e = std::move(b);
    }
    return e;
}
ExprPtr Parser::parse_and() {
    auto e = parse_equality();
    while (match(Tok::AND)) {
        auto b = std::make_unique<Binary>();
        b->line = e->line; b->op = Tok::AND;
        b->lhs = std::move(e); b->rhs = parse_equality();
        e = std::move(b);
    }
    return e;
}
ExprPtr Parser::parse_equality() {
    auto e = parse_relational();
    while (check(Tok::EQ) || check(Tok::NE)) {
        Tok op = advance().type;
        auto b = std::make_unique<Binary>();
        b->line = e->line; b->op = op;
        b->lhs = std::move(e); b->rhs = parse_relational();
        e = std::move(b);
    }
    return e;
}
ExprPtr Parser::parse_relational() {
    auto e = parse_additive();
    while (check(Tok::LT) || check(Tok::GT) || check(Tok::LE) || check(Tok::GE)) {
        Tok op = advance().type;
        auto b = std::make_unique<Binary>();
        b->line = e->line; b->op = op;
        b->lhs = std::move(e); b->rhs = parse_additive();
        e = std::move(b);
    }
    return e;
}
ExprPtr Parser::parse_additive() {
    auto e = parse_multiplicative();
    while (check(Tok::PLUS) || check(Tok::MINUS)) {
        Tok op = advance().type;
        auto b = std::make_unique<Binary>();
        b->line = e->line; b->op = op;
        b->lhs = std::move(e); b->rhs = parse_multiplicative();
        e = std::move(b);
    }
    return e;
}
ExprPtr Parser::parse_multiplicative() {
    auto e = parse_unary();
    while (check(Tok::STAR) || check(Tok::SLASH) || check(Tok::PERCENT)) {
        Tok op = advance().type;
        auto b = std::make_unique<Binary>();
        b->line = e->line; b->op = op;
        b->lhs = std::move(e); b->rhs = parse_unary();
        e = std::move(b);
    }
    return e;
}

ExprPtr Parser::parse_unary() {
    if (check(Tok::KW_SIZEOF)) {
        int line = advance().line;
        expect(Tok::LPAREN, "'('");
        Type t = type_from_name(parse_type());
        expect(Tok::RPAREN, "')'");
        if (t.is_void()) error("sizeof(void) is not allowed");
        auto lit = std::make_unique<IntLit>();
        lit->line = line;
        lit->value = size_of(t);
        return lit;
    }
    if (looks_like_cast()) {
        auto cast = std::make_unique<CastExpr>();
        cast->line = advance().line;   // '('
        cast->type = parse_type();
        expect(Tok::RPAREN, "')'");
        cast->operand = parse_unary();
        return cast;
    }
    if (check(Tok::STAR)) {
        auto d = std::make_unique<DerefExpr>();
        d->line = advance().line;
        d->operand = parse_unary();
        return d;
    }
    if (check(Tok::AMP)) {
        auto a = std::make_unique<AddrOfExpr>();
        a->line = advance().line;
        a->operand = parse_unary();
        return a;
    }
    if (check(Tok::MINUS) || check(Tok::NOT) || check(Tok::TILDE)) {
        Tok op = advance().type;
        auto u = std::make_unique<Unary>();
        u->line = peek().line; u->op = op;
        u->operand = parse_unary();
        return u;
    }
    return parse_postfix();
}

ExprPtr Parser::parse_postfix() {
    auto e = parse_primary();
    while (true) {
        if (check(Tok::LPAREN)) {
            auto* vr = dynamic_cast<VarRef*>(e.get());
            if (!vr) error("call target must be a function name");
            advance();
            auto call = std::make_unique<Call>();
            call->line = e->line;
            call->callee = vr->name;
            if (!check(Tok::RPAREN)) {
                do { call->args.push_back(parse_expr()); } while (match(Tok::COMMA));
            }
            expect(Tok::RPAREN, "')'");
            e = std::move(call);
        } else if (check(Tok::LBRACKET)) {
            advance();
            auto ix = std::make_unique<IndexExpr>();
            ix->line = e->line;
            ix->base = std::move(e);
            ix->index = parse_expr();
            expect(Tok::RBRACKET, "']'");
            e = std::move(ix);
        } else {
            break;
        }
    }
    return e;
}

ExprPtr Parser::parse_primary() {
    const Token& t = peek();
    int line = t.line;
    switch (t.type) {
        case Tok::INT_LIT: {
            auto n = std::make_unique<IntLit>(); n->line = line; n->value = t.int_val;
            advance(); return n;
        }
        case Tok::DOUBLE_LIT: {
            auto d = std::make_unique<DoubleLit>(); d->line = line; d->value = t.dbl_val;
            advance(); return d;
        }
        case Tok::CHAR_LIT: {
            auto c = std::make_unique<CharLit>(); c->line = line; c->value = t.char_val;
            advance(); return c;
        }
        case Tok::KW_TRUE: case Tok::KW_FALSE: {
            auto b = std::make_unique<BoolLit>();
            b->line = line; b->value = (t.type == Tok::KW_TRUE);
            advance(); return b;
        }
        case Tok::IDENT: {
            auto v = std::make_unique<VarRef>(); v->line = line; v->name = t.text;
            advance(); return v;
        }
        case Tok::LPAREN: {
            advance();
            auto e = parse_expr();
            expect(Tok::RPAREN, "')'");
            return e;
        }
        case Tok::STRING_LIT: {
            auto s = std::make_unique<StringLit>(); s->line = line; s->value = t.text;
            advance(); return s;
        }
        default: error("expected expression"); return nullptr;
    }
}