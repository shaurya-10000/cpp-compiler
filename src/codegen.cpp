#include "codegen.h"
#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace {
const char* const kArgRegs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
}

Codegen::Codegen(std::ostream& out) : out_(out), cur_(&out) {}

void Codegen::push_scope() { scopes_.emplace_back(); }
void Codegen::pop_scope() { scopes_.pop_back(); }

void Codegen::allocate_var(const std::string& name) {
    current_offset_ += 8;
    scopes_.back()[name] = current_offset_;
}

int Codegen::lookup_var(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    throw std::runtime_error("Codegen error: undeclared var " + name);
}

std::string Codegen::new_label() { return ".L" + std::to_string(label_count_++); }

std::string Codegen::add_string(const std::string& s) {
    std::string lbl = ".LC" + std::to_string(string_count_++);
    strings_.push_back({lbl, s});
    return lbl;
}

void Codegen::emit_builtins() {
    out_ << R"ASM(.globl print_str
print_str:
  mov %rsi, %rdx
  mov %rdi, %rsi
  mov $1, %rdi
  mov $1, %rax
  syscall
  ret
.globl print_int
print_int:
  sub $32, %rsp
  mov %rdi, %rax
  lea 31(%rsp), %rsi
  movb $10, (%rsi)
  mov $1, %rcx
  mov $10, %r8
  cmp $0, %rax
  jge .L_pos
  neg %rax
  mov $1, %r9
  jmp .L_loop
.L_pos:
  mov $0, %r9
.L_loop:
  xor %rdx, %rdx
  div %r8
  add $48, %dl
  dec %rsi
  mov %dl, (%rsi)
  inc %rcx
  test %rax, %rax
  jnz .L_loop
  cmp $0, %r9
  je .L_print
  dec %rsi
  movb $45, (%rsi)
  inc %rcx
.L_print:
  mov %rcx, %rdx
  mov $1, %rax
  mov $1, %rdi
  syscall
  add $32, %rsp
  ret
)ASM";
}

void Codegen::emit_rodata() {
    out_ << ".section .rodata\n";
    for (auto& pair : strings_) {
        out_ << pair.first << ":\n  .string \"";
        for (unsigned char c : pair.second) {
            if (c == '\n') out_ << "\\n";
            else if (c == '\t') out_ << "\\t";
            else if (c == '"') out_ << "\\\"";
            else if (c == '\\') out_ << "\\\\";
            else if (c < 32 || c > 126) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\%03o", static_cast<unsigned>(c));
                out_ << buf;
            } else {
                out_ << static_cast<char>(c);
            }
        }
        out_ << "\"\n";
    }
}

void Codegen::generate(Program& program) {
    out_ << ".section .note.GNU-stack,\"\",@progbits\n";
    out_ << ".text\n";
    emit_builtins();
    for (auto& fn : program.functions) {
        if (!fn->body) continue;  // prototype only
        gen_function(*fn);
    }
    emit_rodata();
}

void Codegen::gen_function(FuncDecl& fn) {
    current_offset_ = 0;
    push_depth_ = 0;
    current_break_label_.clear();
    current_continue_label_.clear();
    scopes_.clear();
    push_scope();

    // Body is buffered so the exact frame size is known when the prologue is written.
    std::ostringstream body;
    cur_ = &body;

    for (size_t i = 0; i < fn.params.size(); ++i) {
        allocate_var(fn.params[i].name);
        int off = lookup_var(fn.params[i].name);
        if (i < 6) {
            emit() << "  mov " << kArgRegs[i] << ", -" << off << "(%rbp)\n";
        } else {
            // 7th+ args live above the saved rbp and return address
            emit() << "  mov " << (16 + 8 * (i - 6)) << "(%rbp), %rax\n";
            emit() << "  mov %rax, -" << off << "(%rbp)\n";
        }
    }

    gen_block(fn.body.get());

    emit() << "  mov $0, %rax\n";
    emit() << "  leave\n";
    emit() << "  ret\n";
    pop_scope();
    cur_ = &out_;

    int frame = ((current_offset_ + 15) / 16) * 16;
    out_ << ".globl " << fn.name << "\n";
    out_ << fn.name << ":\n";
    out_ << "  push %rbp\n";
    out_ << "  mov %rsp, %rbp\n";
    if (frame > 0) out_ << "  sub $" << frame << ", %rsp\n";
    out_ << body.str();
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
            emit() << "  mov %rax, -" << lookup_var(v->name) << "(%rbp)\n";
        } else {
            allocate_var(v->name);
        }
    } else if (auto* es = dynamic_cast<ExprStmt*>(s)) {
        gen_expr(es->expr.get());
    } else if (auto* r = dynamic_cast<Return*>(s)) {
        if (r->value) gen_expr(r->value.get());
        emit() << "  leave\n";
        emit() << "  ret\n";
    } else if (auto* i = dynamic_cast<If*>(s)) {
        gen_expr(i->cond.get());
        std::string else_lbl = new_label();
        std::string end_lbl = new_label();
        emit() << "  cmp $0, %rax\n";
        emit() << "  je " << (i->else_body ? else_lbl : end_lbl) << "\n";
        gen_block(i->then_body.get());
        emit() << "  jmp " << end_lbl << "\n";
        if (i->else_body) {
            emit() << else_lbl << ":\n";
            gen_block(i->else_body.get());
        }
        emit() << end_lbl << ":\n";
    } else if (dynamic_cast<Break*>(s) != nullptr) {
        if (current_break_label_.empty()) throw std::runtime_error("Codegen error: break outside of loop");
        emit() << "  jmp " << current_break_label_ << "\n";
    } else if (dynamic_cast<Continue*>(s) != nullptr) {
        if (current_continue_label_.empty()) throw std::runtime_error("Codegen error: continue outside of loop");
        emit() << "  jmp " << current_continue_label_ << "\n";
    } else if (auto* f = dynamic_cast<For*>(s)) {
        push_scope();
        if (f->init) gen_stmt(f->init.get());

        std::string start_lbl = new_label();
        std::string end_lbl = new_label();
        std::string step_lbl = new_label();

        std::string old_break = current_break_label_;
        std::string old_continue = current_continue_label_;
        current_break_label_ = end_lbl;
        current_continue_label_ = step_lbl;

        emit() << start_lbl << ":\n";
        if (f->cond) {
            gen_expr(f->cond.get());
            emit() << "  cmp $0, %rax\n";
            emit() << "  je " << end_lbl << "\n";
        }
        gen_block(f->body.get());
        emit() << step_lbl << ":\n";
        if (f->step) gen_expr(f->step.get());
        emit() << "  jmp " << start_lbl << "\n";
        emit() << end_lbl << ":\n";

        current_break_label_ = old_break;
        current_continue_label_ = old_continue;
        pop_scope();
    } else if (auto* w = dynamic_cast<While*>(s)) {
        std::string start_lbl = new_label();
        std::string end_lbl = new_label();

        std::string old_break = current_break_label_;
        std::string old_continue = current_continue_label_;
        current_break_label_ = end_lbl;
        current_continue_label_ = start_lbl;

        emit() << start_lbl << ":\n";
        gen_expr(w->cond.get());
        emit() << "  cmp $0, %rax\n";
        emit() << "  je " << end_lbl << "\n";
        gen_block(w->body.get());
        emit() << "  jmp " << start_lbl << "\n";
        emit() << end_lbl << ":\n";

        current_break_label_ = old_break;
        current_continue_label_ = old_continue;
    } else if (auto* blk = dynamic_cast<Block*>(s)) {
        gen_block(blk);
    }
}

void Codegen::gen_call(Call* call) {
    // print_str with a literal: pass pointer + length directly
    if (call->callee == "print_str" && call->args.size() == 1) {
        if (auto* sl = dynamic_cast<StringLit*>(call->args[0].get())) {
            std::string lbl = add_string(sl->value);
            emit() << "  lea " << lbl << "(%rip), %rdi\n";
            emit() << "  mov $" << sl->value.length() << ", %rsi\n";
            emit() << "  call print_str\n";
            emit() << "  mov $0, %rax\n";
            return;
        }
    }

    const int n = static_cast<int>(call->args.size());
    const int nstack = n > 6 ? n - 6 : 0;
    const int nreg = n < 6 ? n : 6;

    // rsp is 16-byte aligned at push_depth_ == 0. At the call instruction the
    // total number of pushed 8-byte slots must be even.
    const int pad = (push_depth_ + nstack) % 2;
    if (pad) {
        emit() << "  sub $8, %rsp\n";
        push_depth_++;
    }

    // Push right-to-left so arg0 ends up on top and arg6 is the lowest stack arg.
    for (auto it = call->args.rbegin(); it != call->args.rend(); ++it) {
        gen_expr(it->get());
        emit() << "  push %rax\n";
        push_depth_++;
    }
    for (int i = 0; i < nreg; ++i) {
        emit() << "  pop " << kArgRegs[i] << "\n";
        push_depth_--;
    }

    emit() << "  mov $0, %rax\n";
    emit() << "  call " << call->callee << "\n";

    if (nstack + pad > 0) {
        emit() << "  add $" << 8 * (nstack + pad) << ", %rsp\n";
        push_depth_ -= nstack + pad;
    }
}

void Codegen::gen_expr(Expr* e) {
    if (auto* n = dynamic_cast<IntLit*>(e)) {
        emit() << "  mov $" << n->value << ", %rax\n";
    } else if (auto* cl = dynamic_cast<CharLit*>(e)) {
        emit() << "  mov $" << static_cast<int>(cl->value) << ", %rax\n";
    } else if (auto* bl = dynamic_cast<BoolLit*>(e)) {
        emit() << "  mov $" << (bl->value ? 1 : 0) << ", %rax\n";
    } else if (auto* sl = dynamic_cast<StringLit*>(e)) {
        std::string lbl = add_string(sl->value);
        emit() << "  lea " << lbl << "(%rip), %rax\n";
    } else if (auto* v = dynamic_cast<VarRef*>(e)) {
        emit() << "  mov -" << lookup_var(v->name) << "(%rbp), %rax\n";
    } else if (auto* a = dynamic_cast<Assign*>(e)) {
        gen_expr(a->value.get());
        emit() << "  mov %rax, -" << lookup_var(a->name) << "(%rbp)\n";
    } else if (auto* u = dynamic_cast<Unary*>(e)) {
        gen_expr(u->operand.get());
        if (u->op == Tok::MINUS) emit() << "  neg %rax\n";
        else if (u->op == Tok::TILDE) emit() << "  not %rax\n";
        else if (u->op == Tok::NOT) emit() << "  cmp $0, %rax\n  sete %al\n  movzbq %al, %rax\n";
    } else if (auto* bin = dynamic_cast<Binary*>(e)) {
        if (bin->op == Tok::AND || bin->op == Tok::OR) {
            std::string short_lbl = new_label();
            std::string end_lbl = new_label();
            gen_expr(bin->lhs.get());
            emit() << "  cmp $0, %rax\n";
            emit() << (bin->op == Tok::AND ? "  je " : "  jne ") << short_lbl << "\n";
            gen_expr(bin->rhs.get());
            emit() << "  cmp $0, %rax\n  setne %al\n  movzbq %al, %rax\n";
            emit() << "  jmp " << end_lbl << "\n";
            emit() << short_lbl << ":\n";
            emit() << "  mov $" << (bin->op == Tok::AND ? 0 : 1) << ", %rax\n";
            emit() << end_lbl << ":\n";
            return;
        }

        gen_expr(bin->lhs.get());
        emit() << "  push %rax\n";
        push_depth_++;
        gen_expr(bin->rhs.get());
        emit() << "  pop %rcx\n";
        push_depth_--;

        switch (bin->op) {
            case Tok::PLUS:  emit() << "  add %rcx, %rax\n"; break;
            case Tok::MINUS: emit() << "  sub %rax, %rcx\n  mov %rcx, %rax\n"; break;
            case Tok::STAR:  emit() << "  imul %rcx, %rax\n"; break;
            case Tok::SLASH:
                emit() << "  mov %rax, %r8\n  mov %rcx, %rax\n  cqo\n  idiv %r8\n";
                break;
            case Tok::PERCENT:
                emit() << "  mov %rax, %r8\n  mov %rcx, %rax\n  cqo\n  idiv %r8\n  mov %rdx, %rax\n";
                break;
            case Tok::EQ: emit() << "  cmp %rax, %rcx\n  sete %al\n  movzbq %al, %rax\n"; break;
            case Tok::NE: emit() << "  cmp %rax, %rcx\n  setne %al\n  movzbq %al, %rax\n"; break;
            case Tok::LT: emit() << "  cmp %rax, %rcx\n  setl %al\n  movzbq %al, %rax\n"; break;
            case Tok::GT: emit() << "  cmp %rax, %rcx\n  setg %al\n  movzbq %al, %rax\n"; break;
            case Tok::LE: emit() << "  cmp %rax, %rcx\n  setle %al\n  movzbq %al, %rax\n"; break;
            case Tok::GE: emit() << "  cmp %rax, %rcx\n  setge %al\n  movzbq %al, %rax\n"; break;
            default: throw std::runtime_error("Codegen: unsupported binary op");
        }
    } else if (auto* call = dynamic_cast<Call*>(e)) {
        gen_call(call);
    }
}