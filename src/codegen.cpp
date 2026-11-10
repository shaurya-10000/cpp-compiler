#include "codegen.h"
#include "ast_ext.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace {
const char* const kArgRegs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
}

Codegen::Codegen(std::ostream& out, const Sema& sema) : out_(out), cur_(&out), sema_(sema) {}

Type Codegen::type_of(const Expr* e) const {
    auto it = sema_.expr_types().find(e);
    if (it == sema_.expr_types().end())
        throw std::runtime_error("Codegen error: expression has no type information");
    return it->second;
}

void Codegen::push_scope() { scopes_.emplace_back(); }
void Codegen::pop_scope() { scopes_.pop_back(); }

void Codegen::allocate_var(const std::string& name, Type type, int bytes, bool is_array) {
    current_offset_ += bytes;
    scopes_.back()[name] = VarSlot{current_offset_, is_array, type};
}

VarSlot Codegen::lookup_var(const std::string& name) {
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
  mov %rdi, %rsi
  xor %rdx, %rdx
.L_strlen:
  cmpb $0, (%rsi,%rdx)
  je .L_strlen_done
  inc %rdx
  jmp .L_strlen
.L_strlen_done:
  mov $1, %rdi
  mov $1, %rax
  syscall
  ret
.globl print_char
print_char:
  sub $16, %rsp
  mov %dil, (%rsp)
  mov $1, %rax
  mov $1, %rdi
  mov %rsp, %rsi
  mov $1, %rdx
  syscall
  add $16, %rsp
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
    current_ret_ = type_from_name(fn.return_type);
    current_break_label_.clear();
    current_continue_label_.clear();
    scopes_.clear();
    push_scope();

    std::ostringstream body;
    cur_ = &body;

    for (size_t i = 0; i < fn.params.size(); ++i) {
        allocate_var(fn.params[i].name, type_from_name(fn.params[i].type));
        int off = lookup_var(fn.params[i].name).off;
        if (i < 6) {
            emit() << "  mov " << kArgRegs[i] << ", -" << off << "(%rbp)\n";
        } else {
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

// ---------- loads, stores, conversions ----------
// Values live in %rax. Doubles are carried as raw 64-bit patterns.

void Codegen::emit_load_var(const VarSlot& s) {
    if (s.is_array) { emit() << "  lea -" << s.off << "(%rbp), %rax\n"; return; }
    if (s.type == ty::Char) emit() << "  movsbq -" << s.off << "(%rbp), %rax\n";
    else if (s.type == ty::Bool) emit() << "  movzbq -" << s.off << "(%rbp), %rax\n";
    else emit() << "  mov -" << s.off << "(%rbp), %rax\n";
}

void Codegen::emit_load(Type elem) {
    if (elem == ty::Char) emit() << "  movsbq (%rax), %rax\n";
    else if (elem == ty::Bool) emit() << "  movzbq (%rax), %rax\n";
    else emit() << "  mov (%rax), %rax\n";
}

void Codegen::emit_store(Type elem) {
    if (elem == ty::Char || elem == ty::Bool) emit() << "  mov %al, (%rdx)\n";
    else emit() << "  mov %rax, (%rdx)\n";
}

void Codegen::emit_scale(const char* reg, int size) {
    if (size != 1) emit() << "  imul $" << size << ", " << reg << ", " << reg << "\n";
}

void Codegen::emit_to_bool_from_double() {
    emit() << "  movq %rax, %xmm0\n"
              "  xorpd %xmm1, %xmm1\n"
              "  ucomisd %xmm1, %xmm0\n"
              "  setne %al\n"
              "  setp %cl\n"
              "  or %cl, %al\n"
              "  movzbq %al, %rax\n";
}

void Codegen::emit_convert(Type from, Type to) {
    if (from == to || to.is_void() || to.is_ptr()) return;
    if (from.is_ptr()) {   // pointer -> integer (explicit cast)
        if (to == ty::Bool) emit() << "  cmp $0, %rax\n  setne %al\n  movzbq %al, %rax\n";
        else if (to == ty::Char) emit() << "  movsbq %al, %rax\n";
        return;
    }
    if (to.is_double()) {
        emit() << "  cvtsi2sdq %rax, %xmm0\n  movq %xmm0, %rax\n";
        return;
    }
    if (from.is_double()) {
        if (to == ty::Bool) { emit_to_bool_from_double(); return; }
        emit() << "  movq %rax, %xmm0\n  cvttsd2si %xmm0, %rax\n";
        if (to == ty::Char) emit() << "  movsbq %al, %rax\n";
        return;
    }
    if (to == ty::Bool) emit() << "  cmp $0, %rax\n  setne %al\n  movzbq %al, %rax\n";
    else if (to == ty::Char) emit() << "  movsbq %al, %rax\n";
}

void Codegen::gen_cond(Expr* e) {
    gen_expr(e);
    if (type_of(e).is_double()) emit_to_bool_from_double();
    emit() << "  cmp $0, %rax\n";
}

void Codegen::gen_double_binary(Tok op) {
    // %rcx = lhs bits, %rax = rhs bits
    emit() << "  movq %rcx, %xmm0\n  movq %rax, %xmm1\n";
    switch (op) {
        case Tok::PLUS:  emit() << "  addsd %xmm1, %xmm0\n  movq %xmm0, %rax\n"; break;
        case Tok::MINUS: emit() << "  subsd %xmm1, %xmm0\n  movq %xmm0, %rax\n"; break;
        case Tok::STAR:  emit() << "  mulsd %xmm1, %xmm0\n  movq %xmm0, %rax\n"; break;
        case Tok::SLASH: emit() << "  divsd %xmm1, %xmm0\n  movq %xmm0, %rax\n"; break;
        case Tok::EQ:
            emit() << "  ucomisd %xmm1, %xmm0\n  sete %al\n  setnp %cl\n  and %cl, %al\n  movzbq %al, %rax\n"; break;
        case Tok::NE:
            emit() << "  ucomisd %xmm1, %xmm0\n  setne %al\n  setp %cl\n  or %cl, %al\n  movzbq %al, %rax\n"; break;
        case Tok::LT:
            emit() << "  ucomisd %xmm0, %xmm1\n  seta %al\n  movzbq %al, %rax\n"; break;
        case Tok::LE:
            emit() << "  ucomisd %xmm0, %xmm1\n  setae %al\n  movzbq %al, %rax\n"; break;
        case Tok::GT:
            emit() << "  ucomisd %xmm1, %xmm0\n  seta %al\n  movzbq %al, %rax\n"; break;
        case Tok::GE:
            emit() << "  ucomisd %xmm1, %xmm0\n  setae %al\n  movzbq %al, %rax\n"; break;
        default: throw std::runtime_error("Codegen: unsupported double binary op");
    }
}

void Codegen::gen_pointer_binary(Tok op, Type lt, Type rt) {
    // %rcx = lhs, %rax = rhs
    switch (op) {
        case Tok::PLUS:
            if (lt.is_ptr()) emit_scale("%rax", size_of(lt.pointee()));
            else emit_scale("%rcx", size_of(rt.pointee()));
            emit() << "  add %rcx, %rax\n";
            break;
        case Tok::MINUS:
            if (rt.is_ptr()) {   // pointer difference, in elements
                emit() << "  sub %rax, %rcx\n  mov %rcx, %rax\n";
                int sz = size_of(lt.pointee());
                if (sz != 1) emit() << "  cqo\n  mov $" << sz << ", %r8\n  idiv %r8\n";
            } else {
                emit_scale("%rax", size_of(lt.pointee()));
                emit() << "  sub %rax, %rcx\n  mov %rcx, %rax\n";
            }
            break;
        case Tok::EQ: emit() << "  cmp %rax, %rcx\n  sete %al\n  movzbq %al, %rax\n"; break;
        case Tok::NE: emit() << "  cmp %rax, %rcx\n  setne %al\n  movzbq %al, %rax\n"; break;
        case Tok::LT: emit() << "  cmp %rax, %rcx\n  setb %al\n  movzbq %al, %rax\n"; break;
        case Tok::GT: emit() << "  cmp %rax, %rcx\n  seta %al\n  movzbq %al, %rax\n"; break;
        case Tok::LE: emit() << "  cmp %rax, %rcx\n  setbe %al\n  movzbq %al, %rax\n"; break;
        case Tok::GE: emit() << "  cmp %rax, %rcx\n  setae %al\n  movzbq %al, %rax\n"; break;
        default: throw std::runtime_error("Codegen: unsupported pointer binary op");
    }
}

void Codegen::gen_addr(Expr* e) {
    if (auto* v = dynamic_cast<VarRef*>(e)) {
        VarSlot s = lookup_var(v->name);
        emit() << "  lea -" << s.off << "(%rbp), %rax\n";
    } else if (auto* d = dynamic_cast<DerefExpr*>(e)) {
        gen_expr(d->operand.get());
    } else if (auto* ix = dynamic_cast<IndexExpr*>(e)) {
        gen_expr(ix->base.get());
        emit() << "  push %rax\n";
        push_depth_++;
        gen_expr(ix->index.get());
        emit_scale("%rax", size_of(type_of(ix->base.get()).pointee()));
        emit() << "  pop %rcx\n";
        push_depth_--;
        emit() << "  add %rcx, %rax\n";
    } else {
        throw std::runtime_error("Codegen error: expression is not addressable");
    }
}

// ---------- statements ----------

void Codegen::gen_block(Block* b) {
    push_scope();
    for (auto& s : b->stmts) gen_stmt(s.get());
    pop_scope();
}

void Codegen::gen_stmt(Stmt* s) {
    if (auto* av = dynamic_cast<ArrayVarDecl*>(s)) {
        Type elem = type_from_name(av->type);
        long long bytes = ((av->length * size_of(elem) + 7) / 8) * 8;
        allocate_var(av->name, elem.pointer_to(), static_cast<int>(bytes), true);
    } else if (auto* v = dynamic_cast<VarDecl*>(s)) {
        Type vt = type_from_name(v->type);
        if (v->init) {
            gen_expr(v->init.get());
            emit_convert(type_of(v->init.get()), vt);
            allocate_var(v->name, vt);
            emit() << "  mov %rax, -" << lookup_var(v->name).off << "(%rbp)\n";
        } else {
            allocate_var(v->name, vt);
        }
    } else if (auto* es = dynamic_cast<ExprStmt*>(s)) {
        gen_expr(es->expr.get());
    } else if (auto* r = dynamic_cast<Return*>(s)) {
        if (r->value) {
            gen_expr(r->value.get());
            emit_convert(type_of(r->value.get()), current_ret_);
        }
        emit() << "  leave\n";
        emit() << "  ret\n";
    } else if (auto* i = dynamic_cast<If*>(s)) {
        std::string else_lbl = new_label();
        std::string end_lbl = new_label();
        gen_cond(i->cond.get());
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
            gen_cond(f->cond.get());
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
        gen_cond(w->cond.get());
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

// ---------- calls ----------

void Codegen::gen_call(Call* call) {
    const FuncInfo& info = sema_.functions().at(call->callee);
    const int n = static_cast<int>(call->args.size());
    const int nstack = n > 6 ? n - 6 : 0;
    const int nreg = n < 6 ? n : 6;

    // The number of pushed 8-byte slots at the call instruction must be even.
    const int pad = (push_depth_ + nstack) % 2;
    if (pad) {
        emit() << "  sub $8, %rsp\n";
        push_depth_++;
    }

    for (int i = n - 1; i >= 0; --i) {
        Expr* a = call->args[static_cast<size_t>(i)].get();
        gen_expr(a);
        emit_convert(type_of(a), info.params[static_cast<size_t>(i)]);
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

// ---------- expressions ----------

void Codegen::gen_expr(Expr* e) {
    if (auto* n = dynamic_cast<IntLit*>(e)) {
        emit() << "  mov $" << n->value << ", %rax\n";
    } else if (auto* dl = dynamic_cast<DoubleLit*>(e)) {
        std::uint64_t bits;
        std::memcpy(&bits, &dl->value, sizeof bits);
        emit() << "  movabsq $0x" << std::hex << bits << std::dec << ", %rax\n";
    } else if (auto* cl = dynamic_cast<CharLit*>(e)) {
        emit() << "  mov $" << static_cast<int>(cl->value) << ", %rax\n";
    } else if (auto* bl = dynamic_cast<BoolLit*>(e)) {
        emit() << "  mov $" << (bl->value ? 1 : 0) << ", %rax\n";
    } else if (auto* sl = dynamic_cast<StringLit*>(e)) {
        std::string lbl = add_string(sl->value);
        emit() << "  lea " << lbl << "(%rip), %rax\n";
    } else if (auto* v = dynamic_cast<VarRef*>(e)) {
        emit_load_var(lookup_var(v->name));
    } else if (auto* c = dynamic_cast<CastExpr*>(e)) {
        gen_expr(c->operand.get());
        emit_convert(type_of(c->operand.get()), type_of(e));
    } else if (auto* ad = dynamic_cast<AddrOfExpr*>(e)) {
        gen_addr(ad->operand.get());
    } else if (auto* d = dynamic_cast<DerefExpr*>(e)) {
        gen_expr(d->operand.get());
        emit_load(type_of(e));
    } else if (auto* ix = dynamic_cast<IndexExpr*>(e)) {
        gen_addr(ix);
        emit_load(type_of(e));
    } else if (auto* a = dynamic_cast<Assign*>(e)) {
        gen_expr(a->value.get());
        emit_convert(type_of(a->value.get()), type_of(e));
        emit() << "  mov %rax, -" << lookup_var(a->name).off << "(%rbp)\n";
    } else if (auto* st = dynamic_cast<StoreExpr*>(e)) {
        Type elem = type_of(e);
        gen_addr(st->target.get());
        emit() << "  push %rax\n";
        push_depth_++;
        gen_expr(st->value.get());
        emit_convert(type_of(st->value.get()), elem);
        emit() << "  pop %rdx\n";
        push_depth_--;
        emit_store(elem);
    } else if (auto* u = dynamic_cast<Unary*>(e)) {
        gen_expr(u->operand.get());
        Type t = type_of(u->operand.get());
        if (u->op == Tok::MINUS) {
            if (t.is_double()) emit() << "  movabsq $0x8000000000000000, %rcx\n  xor %rcx, %rax\n";
            else emit() << "  neg %rax\n";
        } else if (u->op == Tok::TILDE) {
            emit() << "  not %rax\n";
        } else if (u->op == Tok::NOT) {
            if (t.is_double()) emit_to_bool_from_double();
            emit() << "  cmp $0, %rax\n  sete %al\n  movzbq %al, %rax\n";
        }
    } else if (auto* bin = dynamic_cast<Binary*>(e)) {
        if (bin->op == Tok::AND || bin->op == Tok::OR) {
            std::string short_lbl = new_label();
            std::string end_lbl = new_label();
            gen_cond(bin->lhs.get());
            emit() << (bin->op == Tok::AND ? "  je " : "  jne ") << short_lbl << "\n";
            gen_cond(bin->rhs.get());
            emit() << "  setne %al\n  movzbq %al, %rax\n";
            emit() << "  jmp " << end_lbl << "\n";
            emit() << short_lbl << ":\n";
            emit() << "  mov $" << (bin->op == Tok::AND ? 0 : 1) << ", %rax\n";
            emit() << end_lbl << ":\n";
            return;
        }

        Type lt = type_of(bin->lhs.get());
        Type rt = type_of(bin->rhs.get());
        const bool ptr_op = lt.is_ptr() || rt.is_ptr();
        const bool dbl = !ptr_op && (lt.is_double() || rt.is_double());

        gen_expr(bin->lhs.get());
        if (dbl) emit_convert(lt, ty::Double);
        emit() << "  push %rax\n";
        push_depth_++;
        gen_expr(bin->rhs.get());
        if (dbl) emit_convert(rt, ty::Double);
        emit() << "  pop %rcx\n";
        push_depth_--;

        if (dbl) { gen_double_binary(bin->op); return; }
        if (ptr_op) { gen_pointer_binary(bin->op, lt, rt); return; }

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