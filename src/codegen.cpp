#include "codegen.h"
#include <iostream>
#include <stdexcept>

Codegen::Codegen(std::ostream& out) : out_(out) {}

void Codegen::push_scope() { scopes_.emplace_back(); }
void Codegen::pop_scope() { scopes_.pop_back(); }

void Codegen::allocate_var(const std::string& name) {
    current_offset_ += 8;
    scopes_.back()[name] = current_offset_;
}

int Codegen::lookup_var(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->count(name)) return it->at(name);
    }
    throw std::runtime_error("Codegen error: undeclared var " + name);
}

std::string Codegen::new_label() {
    return ".L" + std::to_string(label_count_++);
}

void Codegen::generate(Program& program) {
    out_ << ".text\n";
    for (auto& fn : program.functions) {
        current_offset_ = 0;
        scopes_.clear();
        push_scope();

        out_ << ".globl " << fn->name << "\n";
        out_ << fn->name << ":\n";
        out_ << "  push %rbp\n";
        out_ << "  mov %rsp, %rbp\n";
        
        // We need a fixed stack size. To keep it simple, we allocate a fixed 
        // amount of space for all local variables (we'll just use a large buffer).
        // In a real compiler, we'd do a pre-pass to count exact stack size.
        out_ << "  sub $256, %rsp\n"; 

        // Parameters (System V: rdi, rsi, rdx, rcx, r8, r9)
        const char* regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
        for (size_t i = 0; i < fn->params.size() && i < 6; ++i) {
            allocate_var(fn->params[i].name);
            out_ << "  mov " << regs[i] << ", -" << lookup_var(fn->params[i].name) << "(%rbp)\n";
        }

        gen_block(fn->body.get());

        // Default return 0 if void or missing return
        out_ << "  mov $0, %rax\n"; 
        out_ << "  leave\n";
        out_ << "  ret\n";
        
        pop_scope();
    }
}

void Codegen::gen_block(Block* b) {
    push_scope();
    for (auto& s : b->stmts) gen_stmt(s.get());
    pop_scope();
}

void Codegen::gen_stmt(Stmt* s) {
    if (auto* v = dynamic_cast<VarDecl*>(s)) {
        if (v->init) {
            gen_expr(v->init.get());
            allocate_var(v->name);
            out_ << "  mov %rax, -" << lookup_var(v->name) << "(%rbp)\n";
        } else {
            allocate_var(v->name);
        }
    } else if (auto* e = dynamic_cast<ExprStmt*>(s)) {
        gen_expr(e->expr.get());
    } else if (auto* r = dynamic_cast<Return*>(s)) {
        if (r->value) gen_expr(r->value.get());
        out_ << "  leave\n";
        out_ << "  ret\n";
    } else if (auto* i = dynamic_cast<If*>(s)) {
        gen_expr(i->cond.get());
        std::string else_lbl = new_label();
        std::string end_lbl = new_label();
        
        out_ << "  cmp $0, %rax\n";
        out_ << "  je " << (i->else_body ? else_lbl : end_lbl) << "\n";
        
        gen_block(i->then_body.get());
        out_ << "  jmp " << end_lbl << "\n";
        
        if (i->else_body) {
            out_ << else_lbl << ":\n";
            gen_block(i->else_body.get());
        }
        out_ << end_lbl << ":\n";
    } else if (auto* w = dynamic_cast<While*>(s)) {
        std::string start_lbl = new_label();
        std::string end_lbl = new_label();
        out_ << start_lbl << ":\n";
        gen_expr(w->cond.get());
        out_ << "  cmp $0, %rax\n";
        out_ << "  je " << end_lbl << "\n";
        gen_block(w->body.get());
        out_ << "  jmp " << start_lbl << "\n";
        out_ << end_lbl << ":\n";
    } else if (auto* b = dynamic_cast<Block*>(s)) {
        gen_block(b);
    }
}

void Codegen::gen_expr(Expr* e) {
    if (auto* n = dynamic_cast<IntLit*>(e)) {
        out_ << "  mov $" << n->value << ", %rax\n";
    } else if (auto* c = dynamic_cast<CharLit*>(e)) {
        out_ << "  mov $" << (int)c->value << ", %rax\n";
    } else if (auto* b = dynamic_cast<BoolLit*>(e)) {
        out_ << "  mov $" << (b->value ? 1 : 0) << ", %rax\n";
    } else if (auto* v = dynamic_cast<VarRef*>(e)) {
        out_ << "  mov -" << lookup_var(v->name) << "(%rbp), %rax\n";
    } else if (auto* a = dynamic_cast<Assign*>(e)) {
        gen_expr(a->value.get());
        out_ << "  mov %rax, -" << lookup_var(a->name) << "(%rbp)\n";
    } else if (auto* u = dynamic_cast<Unary*>(e)) {
        gen_expr(u->operand.get());
        if (u->op == Tok::MINUS) out_ << "  neg %rax\n";
        else if (u->op == Tok::NOT) out_ << "  cmp $0, %rax\n  sete %al\n  movzb %al, %rax\n";
    } else if (auto* b = dynamic_cast<Binary*>(e)) {
        gen_expr(b->lhs.get());
        out_ << "  push %rax\n";
        gen_expr(b->rhs.get());
        out_ << "  pop %rcx\n"; // rcx = lhs, rax = rhs

        switch (b->op) {
            case Tok::PLUS:    out_ << "  add %rcx, %rax\n"; break;
            case Tok::MINUS:   out_ << "  sub %rax, %rcx\n  mov %rcx, %rax\n"; break; // rhs - lhs? wait: rcx (lhs) - rax (rhs). we want lhs - rhs
            case Tok::STAR:    out_ << "  imul %rcx, %rax\n"; break;
            case Tok::SLASH:   
                out_ << "  mov %rax, %r8\n"; // divisor
                out_ << "  mov %rcx, %rax\n"; // dividend
                out_ << "  cqo\n";
                out_ << "  idiv %r8\n"; 
                break;
            case Tok::EQ:      out_ << "  cmp %rax, %rcx\n  sete %al\n  movzb %al, %rax\n"; break;
            case Tok::NE:      out_ << "  cmp %rax, %rcx\n  setne %al\n  movzb %al, %rax\n"; break;
            case Tok::LT:      out_ << "  cmp %rax, %rcx\n  setl %al\n  movzb %al, %rax\n"; break;
            case Tok::GT:      out_ << "  cmp %rax, %rcx\n  setg %al\n  movzb %al, %rax\n"; break;
            case Tok::LE:      out_ << "  cmp %rax, %rcx\n  setle %al\n  movzb %al, %rax\n"; break;
            case Tok::GE:      out_ << "  cmp %rax, %rcx\n  setge %al\n  movzb %al, %rax\n"; break;
            default: throw std::runtime_error("Codegen: unsupported binary op");
        }
    } else if (auto* c = dynamic_cast<Call*>(e)) {
        // Evaluate args in reverse order for stack
        for (auto it = c->args.rbegin(); it != c->args.rend(); ++it) {
            gen_expr(it->get());
            out_ << "  push %rax\n";
        }
        
        // Pop into registers (System V: rdi, rsi, rdx...)
        const char* regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
        for (size_t i = 0; i < c->args.size() && i < 6; ++i) {
            out_ << "  pop " << regs[i] << "\n";
        }
        
        out_ << "  mov $0, %rax\n"; // 0 vector regs for varargs
        out_ << "  call " << c->callee << "\n";
        
        // Clean up stack if > 6 args (not implemented for simplicity)
    }
}
