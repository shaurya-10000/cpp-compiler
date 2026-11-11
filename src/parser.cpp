#include "parser.h"
#include "ast_ext.h"
#include "types.h"
#include <stdexcept>

namespace {
const std::vector<std::vector<Tok>>& binary_levels() {
    static const std::vector<std::vector<Tok>> levels = {
        {Tok::OR}, {Tok::AND}, {Tok::PIPE}, {Tok::CARET}, {Tok::AMP},
        {Tok::EQ, Tok::NE}, {Tok::LT, Tok::GT, Tok::LE, Tok::GE},
        {Tok::SHL, Tok::SHR}, {Tok::PLUS, Tok::MINUS},
        {Tok::STAR, Tok::SLASH, Tok::PERCENT},
    };
    return levels;
}

bool compound_base(Tok t, Tok& base) {
    switch (t) {
        case Tok::PLUS_ASSIGN:    base = Tok::PLUS; return true;
        case Tok::MINUS_ASSIGN:   base = Tok::MINUS; return true;
        case Tok::STAR_ASSIGN:    base = Tok::STAR; return true;
        case Tok::SLASH_ASSIGN:   base = Tok::SLASH; return true;
        case Tok::PERCENT_ASSIGN: base = Tok::PERCENT; return true;
        case Tok::AMP_ASSIGN:     base = Tok::AMP; return true;
        case Tok::PIPE_ASSIGN:    base = Tok::PIPE; return true;
        case Tok::CARET_ASSIGN:   base = Tok::CARET; return true;
        case Tok::SHL_ASSIGN:     base = Tok::SHL; return true;
        case Tok::SHR_ASSIGN:     base = Tok::SHR; return true;
        default: return false;
    }
}
}

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
bool Parser::is_type_start() const { return is_type_kw(peek().type) || check(Tok::KW_STRUCT); }

bool Parser::is_lvalue(const Expr* e) const {
    return dynamic_cast<const VarRef*>(e) != nullptr || dynamic_cast<const DerefExpr*>(e) != nullptr ||
           dynamic_cast<const IndexExpr*>(e) != nullptr || dynamic_cast<const MemberExpr*>(e) != nullptr;
}

// type := (int|bool|char|double|void | struct Name) '*'*     e.g. "char*", "struct Node*"
std::string Parser::parse_type() {
    std::string name;
    if (check(Tok::KW_STRUCT)) {
        advance();
        std::string sn = expect(Tok::IDENT, "struct name").text;
        if (find_struct(sn) < 0) error("unknown struct '" + sn + "'");
        name = "struct " + sn;
    } else if (is_type_kw(peek().type)) {
        name = advance().text;
    } else {
        error("expected type");
    }
    while (match(Tok::STAR)) name += '*';
    return name;
}

// '(' type ')'
bool Parser::looks_like_cast() const {
    if (!check(Tok::LPAREN)) return false;
    int k = 1;
    if (peek(k).type == Tok::KW_STRUCT) {
        if (peek(k + 1).type != Tok::IDENT) return false;
        k += 2;
    } else if (is_type_kw(peek(k).type)) {
        k++;
    } else {
        return false;
    }
    while (peek(k).type == Tok::STAR) k++;
    return peek(k).type == Tok::RPAREN;
}

void Parser::parse_struct_decl() {
    advance();   // 'struct'
    std::string name = expect(Tok::IDENT, "struct name").text;
    if (match(Tok::SEMICOLON)) {   // forward declaration
        if (find_struct(name) < 0) {
            StructDef d;
            d.name = name;
            struct_defs().push_back(d);
        }
        return;
    }
    expect(Tok::LBRACE, "'{'");
    int sid = find_struct(name);
    if (sid >= 0 && struct_defs()[static_cast<size_t>(sid)].complete)
        error("redefinition of struct '" + name + "'");
    if (sid < 0) {
        StructDef d;
        d.name = name;
        struct_defs().push_back(d);
        sid = static_cast<int>(struct_defs().size()) - 1;
    }

    std::vector<StructField> fields;
    int offset = 0, max_align = 1;
    while (!check(Tok::RBRACE) && !check(Tok::END)) {
        std::string tname = parse_type();
        std::string fname = expect(Tok::IDENT, "member name").text;
        if (check(Tok::LBRACKET)) error("array members are not supported");
        expect(Tok::SEMICOLON, "';'");
        Type ft = type_from_name(tname);
        if (ft.is_void()) error("member '" + fname + "' cannot have type void");
        if (ft.is_struct() && !struct_defs()[static_cast<size_t>(ft.sid)].complete)
            error("member '" + fname + "' has incomplete type '" + type_name(ft) + "'");
        for (const auto& f : fields)
            if (f.name == fname) error("duplicate member '" + fname + "'");
        int al = align_of(ft);
        offset = (offset + al - 1) / al * al;
        fields.push_back(StructField{fname, ft, offset});
        offset += size_of(ft);
        if (al > max_align) max_align = al;
    }
    expect(Tok::RBRACE, "'}'");
    expect(Tok::SEMICOLON, "';'");
    if (fields.empty()) error("struct '" + name + "' has no members");

    StructDef& def = struct_defs()[static_cast<size_t>(sid)];
    def.fields = std::move(fields);
    def.align = max_align;
    def.size = (offset + max_align - 1) / max_align * max_align;
    def.complete = true;
}

// '[' (INT)? ']' ( '=' ( '{' expr (',' expr)* ','? '}' | STRING ) )?
void Parser::parse_array_suffix(long long& length, ArrayInit& ai) {
    expect(Tok::LBRACKET, "'['");
    if (check(Tok::RBRACKET)) length = -1;
    else length = expect(Tok::INT_LIT, "array size").int_val;
    expect(Tok::RBRACKET, "']'");
    if (match(Tok::ASSIGN)) {
        if (match(Tok::LBRACE)) {
            ai.has_list = true;
            if (!check(Tok::RBRACE)) {
                do {
                    if (check(Tok::RBRACE)) break;
                    ai.list.push_back(parse_assign());
                } while (match(Tok::COMMA));
            }
            expect(Tok::RBRACE, "'}'");
        } else if (check(Tok::STRING_LIT)) {
            ai.has_str = true;
            ai.str = advance().text;
        } else {
            error("array initializer must be a brace list or string literal");
        }
    }
}

Program Parser::parse() {
    Program p;
    while (!check(Tok::END)) {
        if (check(Tok::KW_STRUCT) &&
            (peek(2).type == Tok::LBRACE || peek(2).type == Tok::SEMICOLON)) {
            parse_struct_decl();
            continue;
        }
        int line = peek().line;
        std::string type = parse_type();
        std::string name = expect(Tok::IDENT, "name").text;
        if (check(Tok::LPAREN)) p.functions.push_back(parse_function_rest(type, name, line));
        else parse_global_rest(type, name, line);
    }
    return p;
}

void Parser::parse_global_rest(const std::string& type, const std::string& name, int line) {
    auto g = std::make_unique<GlobalDecl>();
    g->line = line;
    g->type = type;
    g->name = name;
    if (check(Tok::LBRACKET)) {
        g->is_array = true;
        parse_array_suffix(g->length, g->init_data);
    } else if (match(Tok::ASSIGN)) {
        g->init = parse_assign();
    }
    expect(Tok::SEMICOLON, "';'");
    globals_.push_back(std::move(g));
}

std::unique_ptr<FuncDecl> Parser::parse_function_rest(const std::string& ret, const std::string& name, int line) {
    auto fn = std::make_unique<FuncDecl>();
    fn->line = line;
    fn->return_type = ret;
    fn->name = name;

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
    if (is_type_start()) return parse_var_decl();
    if (check(Tok::KW_IF)) return parse_if();
    if (check(Tok::KW_WHILE)) return parse_while();
    if (check(Tok::KW_DO)) return parse_do_while();
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

    if (check(Tok::LBRACKET)) {
        auto a = std::make_unique<ArrayVarDecl>();
        a->line = line;
        a->type = type;
        a->name = name;
        parse_array_suffix(a->length, a->init_data);
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

StmtPtr Parser::parse_do_while() {
    auto s = std::make_unique<DoWhile>();
    s->line = advance().line;   // 'do'
    s->body = parse_block();
    expect(Tok::KW_WHILE, "'while'");
    expect(Tok::LPAREN, "'('");
    s->cond = parse_expr();
    expect(Tok::RPAREN, "')'");
    expect(Tok::SEMICOLON, "';'");
    return s;
}

StmtPtr Parser::parse_for() {
    auto s = std::make_unique<For>();
    s->line = advance().line;
    expect(Tok::LPAREN, "'('");

    if (check(Tok::SEMICOLON)) {
        advance();
    } else if (is_type_start()) {
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
    auto lhs = parse_ternary();
    if (check(Tok::ASSIGN)) {
        advance();
        int line = lhs->line;
        if (auto* vr = dynamic_cast<VarRef*>(lhs.get())) {
            auto a = std::make_unique<Assign>();
            a->line = line;
            a->name = vr->name;
            a->value = parse_assign();   // right associative
            return a;
        }
        if (is_lvalue(lhs.get())) {
            auto st = std::make_unique<StoreExpr>();
            st->line = line;
            st->target = std::move(lhs);
            st->value = parse_assign();
            return st;
        }
        error("invalid assignment target");
    }
    Tok base = Tok::PLUS;
    if (compound_base(peek().type, base)) {
        advance();
        if (!is_lvalue(lhs.get())) error("invalid assignment target");
        auto c = std::make_unique<CompoundExpr>();
        c->line = lhs->line;
        c->op = base;
        c->target = std::move(lhs);
        c->value = parse_assign();
        return c;
    }
    return lhs;
}

ExprPtr Parser::parse_ternary() {
    auto cond = parse_binary(0);
    if (!match(Tok::QUESTION)) return cond;
    auto c = std::make_unique<CondExpr>();
    c->line = cond->line;
    c->cond = std::move(cond);
    c->then_e = parse_expr();
    expect(Tok::COLON, "':'");
    c->else_e = parse_ternary();
    return c;
}

ExprPtr Parser::parse_binary(size_t level) {
    const auto& levels = binary_levels();
    if (level >= levels.size()) return parse_unary();
    auto e = parse_binary(level + 1);
    while (true) {
        bool found = false;
        for (Tok t : levels[level]) {
            if (check(t)) { found = true; break; }
        }
        if (!found) break;
        Tok op = advance().type;
        auto b = std::make_unique<Binary>();
        b->line = e->line;
        b->op = op;
        b->lhs = std::move(e);
        b->rhs = parse_binary(level + 1);
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
        if (t.is_struct() && !struct_defs()[static_cast<size_t>(t.sid)].complete)
            error("sizeof applied to incomplete type '" + type_name(t) + "'");
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
    if (check(Tok::PLUSPLUS) || check(Tok::MINUSMINUS)) {
        bool inc = check(Tok::PLUSPLUS);
        int line = advance().line;
        auto t = parse_unary();
        if (!is_lvalue(t.get())) error("operand of '++'/'--' must be an lvalue");
        auto id = std::make_unique<IncDecExpr>();
        id->line = line;
        id->inc = inc;
        id->prefix = true;
        id->target = std::move(t);
        return id;
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
        } else if (check(Tok::DOT) || check(Tok::ARROW)) {
            bool arrow = check(Tok::ARROW);
            advance();
            auto m = std::make_unique<MemberExpr>();
            m->line = e->line;
            m->arrow = arrow;
            m->name = expect(Tok::IDENT, "member name").text;
            m->base = std::move(e);
            e = std::move(m);
        } else if (check(Tok::PLUSPLUS) || check(Tok::MINUSMINUS)) {
            if (!is_lvalue(e.get())) error("operand of '++'/'--' must be an lvalue");
            bool inc = check(Tok::PLUSPLUS);
            advance();
            auto id = std::make_unique<IncDecExpr>();
            id->line = e->line;
            id->inc = inc;
            id->prefix = false;
            id->target = std::move(e);
            e = std::move(id);
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