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

std::string Codegen::add_string(const std::string& s) {
    std::string lbl = ".LC" + std::to_string(string_count_++);
    strings_.push_back({lbl, s});
    return lbl;
}

void Codegen::generate(Program& program) {
    out_ << ".text\n";
    
    // --- Built-in: print_str(char* buf, long len) ---
    // RDI = buffer, RSI = length
    out_ << ".globl print_str\n";
    out_ << "print_str:\n";
    out_ << "  mov %rsi, %rdx\n"; // length
    out_ << "  mov %rdi, %rsi\n"; // buffer
    out_ << "  mov $1, %rdi\n";   // stdout
    out_ << "  mov $1, %rax\n";   // sys_write
    out_ << "  syscall\n";
    out_ << "  ret\n";

    // --- Built-in: print_int(long n) ---
    out_ << ".globl print_int\n";
    out_ << "print_int:\n";
    out_ << "  sub $32, %rsp\n";       // buffer on stack
    out_ << "  mov %rdi, %rax\n";      // number in rax
    out_ << "  lea 31(%rsp), %rsi\n";  // pointer to end of buffer
    out_ << "  movb $10, (%rsi)\n";    // newline
    out_ << "  mov $1, %rcx\n";        // length = 1 (for newline)
    out_ << "  mov $10, %r8\n";        // divisor
    out_ << "  cmp $0, %rax\n";
    out_ << "  jge .L_pos\n";
    out_ << "  neg %rax\n";            // make positive for conversion
    out_ << "  mov $1, %r9\n";         // negative flag
    out_ << "  jmp .L_loop\n";
    out_ << ".L_pos:\n";
    out_ << "  mov $0, %r9\n";
    out_ << ".L_loop:\n";
    out_ << "  xor %rdx, %rdx\n";
    out_ << "  div %r8\n";             // rax = quotient, rdx = remainder
    out_ << "  add $48, %dl\n";        // convert to ASCII
    out_ << "  dec %rsi\n";
    out_ << "  mov %dl, (%rsi)\n";
    out_ << "  inc %rcx\n";
    out_ << "  test %rax, %rax\n";
    out_ << "  jnz .L_loop\n";
    out_ << "  cmp $0, %r9\n";
    out_ << "  je .L_print\n";
    out_ << "  dec %rsi\n";
    out_ << "  movb $45, (%rsi)\n";    // '-' sign
    out_ << "  inc %rcx\n";
    out_ << ".L_print:\n";
    out_ << "  mov %rcx, %rdx\n";      // length
    out_ << "  mov $1, %rax\n";        // sys_write
    out_ << "  mov $1, %rdi\n";        // stdout
    out_ << "  syscall\n";
    out_ << "  add $32, %rsp\n";
    out_ << "  ret\n";

    for (auto& fn : program.functions) {
        current_offset_ = 0;
        scopes_.clear();
        push_scope();

        out_ << ".globl " << fn->name << "\n";
        out_ << fn->name << ":\n";
        out_ << "  push %rbp\n";
        out_ << "  mov %rsp, %rbp\n";
        out_ << "  sub $256, %rsp\n"; 

        const char* regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
        for (size_t i = 0; i < fn->params.size() && i < 6; ++i) {
            allocate_var(fn->params[i].name);
            out_ << "  mov " << regs[i] << ", -" << lookup_var(fn->params[i].name) << "(%rbp)\n";
        }

        gen_block(fn->body.get());

        out_ << "  mov $0, %rax\n"; 
        out_ << "  leave\n";
        out_ << "  ret\n";
        
        pop_scope();
    }

    out_ << ".section .rodata\n";
    for (auto& pair : strings_) {
        out_ << pair.first << ":\n";
                out_ << "  .string \"";
        for (char c : pair.second) {
            if (c == '\n') out_ << "\\n";
            else if (c == '\t') out_ << "\\t";
            else if (c == '\"') out_ << "\\\"";
            else if (c == '\\') out_ << "\\\\";
            else out_ << c;
        }
        out_ << "\"\n";
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
        } else if (auto* b = dynamic_cast<Break*>(s)) {
        if (current_break_label_.empty()) throw std::runtime_error("Codegen error: break outside of loop");
        out_ << "  jmp " << current_break_label_ << "\n";
    } else if (auto* c = dynamic_cast<Continue*>(s)) {
        if (current_continue_label_.empty()) throw std::runtime_error("Codegen error: continue outside of loop");
        out_ << "  jmp " << current_continue_label_ << "\n";
    } else if (auto* f = dynamic_cast<For*>(s)) {
        push_scope();
        if (f->init) gen_stmt(f->init.get());
        
        std::string start_lbl = new_label();
        std::string end_lbl = new_label();
        std::string step_lbl = new_label(); // Continue jumps here
        
        std::string old_break = current_break_label_;
        std::string old_continue = current_continue_label_;
        current_break_label_ = end_lbl;
        current_continue_label_ = step_lbl;

        out_ << start_lbl << ":\n";
        if (f->cond) {
            gen_expr(f->cond.get());
            out_ << "  cmp     } else if (auto* w = dynamic_cast<While*>(s)) {
        std::string start_lbl = new_label();
        std::string end_lbl = new_label();
        
        std::string old_break = current_break_label_;
        std::string old_continue = current_continue_label_;
        current_break_label_ = end_lbl;
        current_continue_label_ = start_lbl;

        out_ << start_lbl << ":\n";
        gen_expr(w->cond.get());
        out_ << "  cmp } else if (auto* w = dynamic_cast<While*>(s)) {, %rax\n";
            out_ << "  je " << end_lbl << "\n";
        }
        
        gen_block(f->body.get());

        out_ << step_lbl << ":\n";
        if (f->step) gen_expr(f->step.get());
        out_ << "  jmp " << start_lbl << "\n";
        out_ << end_lbl << ":\n";

        current_break_label_ = old_break;
        current_continue_label_ = old_continue;
        pop_scope();
    }
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
    } else if (auto* b = dynamic_cast<Block*>(s)) {, %rax\n";
        out_ << "  je " << end_lbl << "\n";
        
        gen_block(w->body.get());
        
        out_ << "  jmp " << start_lbl << "\n";
        out_ << end_lbl << ":\n";
        
        current_break_label_ = old_break;
        current_continue_label_ = old_continue;
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
    } else if (auto* s = dynamic_cast<StringLit*>(e)) {
        std::string lbl = add_string(s->value);
        out_ << "  lea " << lbl << "(%rip), %rax\n";
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
        out_ << "  pop %rcx\n"; 

        switch (b->op) {
            case Tok::PLUS:    out_ << "  add %rcx, %rax\n"; break;
            case Tok::MINUS:   out_ << "  sub %rax, %rcx\n  mov %rcx, %rax\n"; break;
            case Tok::STAR:    out_ << "  imul %rcx, %rax\n"; break;
            case Tok::SLASH:   
                out_ << "  mov %rax, %r8\n"; 
                out_ << "  mov %rcx, %rax\n";
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
        // Special case for print_str
        if (c->callee == "print_str" && c->args.size() == 1) {
            if (auto* s = dynamic_cast<StringLit*>(c->args[0].get())) {
                std::string lbl = add_string(s->value);
                out_ << "  lea " << lbl << "(%rip), %rdi\n";
                out_ << "  mov $" << s->value.length() << ", %rsi\n";
                out_ << "  call print_str\n";
                out_ << "  mov $0, %rax\n"; // Return 0
                return;
            }
        }

        // General function call
        for (auto it = c->args.rbegin(); it != c->args.rend(); ++it) {
            gen_expr(it->get());
            out_ << "  push %rax\n";
        }
        
        const char* regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
        for (size_t i = 0; i < c->args.size() && i < 6; ++i) {
            out_ << "  pop " << regs[i] << "\n";
        }
        
        out_ << "  mov $0, %rax\n"; 
        out_ << "  call " << c->callee << "\n";
    }
}



