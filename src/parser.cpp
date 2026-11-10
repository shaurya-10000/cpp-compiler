#include "parser.h"
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
    
    // Type
    if (!match(Tok::KW_INT) && !match(Tok::KW_VOID) && !match(Tok::KW_BOOL) && !match(Tok::KW_CHAR))
        error("expected return type");
    fn->return_type = toks_[pos_-1].text;

    // Name
    fn->name = expect(Tok::IDENT, "function name").text;
    
    // Params
    expect(Tok::LPAREN, "'('");
    if (!check(Tok::RPAREN)) {
        do {
            Param p;
            if (!match(Tok::KW_INT) && !match(Tok::KW_BOOL) && !match(Tok::KW_CHAR))
                error("expected parameter type");
            p.type = toks_[pos_-1].text;
            p.name = expect(Tok::IDENT, "parameter name").text;
            fn->params.push_back(p);
        } while (match(Tok::COMMA));
    }
    expect(Tok::RPAREN, "')'");

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
    if (check(Tok::KW_INT) || check(Tok::KW_BOOL) || check(Tok::KW_CHAR)) return parse_var_decl();
        if (check(Tok::KW_IF)) return parse_if();
    if (check(Tok::KW_WHILE)) return parse_while();
    if (check(Tok::KW_FOR)) return parse_for();
    if (check(Tok::KW_BREAK)) { 
        advance(); 
        auto b = std::make_unique<Break>(); 
        b->line = toks_[pos_-1].line; 
        expect(Tok::SEMICOLON, "';'");
        return b; 
    }
    if (check(Tok::KW_CONTINUE)) { 
        advance(); 
        auto c = std::make_unique<Continue>(); 
        c->line = toks_[pos_-1].line; 
        expect(Tok::SEMICOLON, "';'");
        return c; 
    }
    if (check(Tok::KW_RETURN)) return parse_return();
    if (check(Tok::LBRACE)) {
        auto b = parse_block();
        // Block is a statement, but in this simple AST we just return it
        return b; 
    }

    // ExprStmt
    auto s = std::make_unique<ExprStmt>();
    s->line = peek().line;
    s->expr = parse_expr();
    expect(Tok::SEMICOLON, "';'");
    return s;
}

StmtPtr Parser::parse_var_decl() {
    auto s = std::make_unique<VarDecl>();
    s->line = peek().line;
    advance(); // type keyword
    s->type = toks_[pos_-1].text;
    s->name = expect(Tok::IDENT, "variable name").text;
    if (match(Tok::ASSIGN)) {
        s->init = parse_expr();
    }
    expect(Tok::SEMICOLON, "';'");
    return s;
}

StmtPtr Parser::parse_if() {
    auto s = std::make_unique<If>();
    s->line = advance().line; // 'if'
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
    s->line = advance().line; // 'while'
    expect(Tok::LPAREN, "'('");
    s->cond = parse_expr();
    expect(Tok::RPAREN, "')'");
    s->body = parse_block();
    return s;
}

StmtPtr Parser::parse_for() {
    auto s = std::make_unique<For>();
    s->line = advance().line; // 'for'
    expect(Tok::LPAREN, "'('");
    
    // Init
    if (check(Tok::SEMICOLON)) {
        advance();
    } else if (check(Tok::KW_INT) || check(Tok::KW_BOOL) || check(Tok::KW_CHAR)) {
        s->init = parse_var_decl();
    } else {
        auto e = std::make_unique<ExprStmt>();
        e->line = peek().line;
        e->expr = parse_expr();
        expect(Tok::SEMICOLON, "';'");
        s->init = std::move(e);
    }

    // Cond
    if (!check(Tok::SEMICOLON)) {
        s->cond = parse_expr();
    }
    expect(Tok::SEMICOLON, "';'");

    // Step
    if (!check(Tok::RPAREN)) {
        s->step = parse_expr();
    }
    expect(Tok::RPAREN, "')'");

    s->body = parse_block();
    return s;
}

StmtPtr Parser::parse_return() {
    auto s = std::make_unique<Return>();
    s->line = advance().line; // 'return'
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
        auto* vr = dynamic_cast<VarRef*>(lhs.get());
        if (!vr) error("invalid assignment target");
        auto a = std::make_unique<Assign>();
        a->line = lhs->line;
        a->name = vr->name;
        a->value = parse_assign(); // right assoc
        return a;
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
    while (check(Tok::LPAREN)) {
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
        }        default: error("expected expression"); return nullptr;
    }
}


