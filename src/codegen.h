#pragma once
#include "ast.h"
#include "ast_ext.h"
#include "sema.h"
#include "types.h"
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct VarSlot {
    int off = 0;               // distance below %rbp of the slot's lowest address (locals)
    bool is_array = false;
    Type type;                 // for arrays: pointer to element type
    bool is_global = false;
    std::string sym;           // variable name (used for globals)
};

class Codegen {
public:
    Codegen(std::ostream& out, const Sema& sema);
    void generate(Program& program, Globals& globals);

private:
    std::ostream& out_;
    std::ostream* cur_;
    const Sema& sema_;
    std::vector<std::unordered_map<std::string, VarSlot>> scopes_;
    std::unordered_map<std::string, VarSlot> globals_;
    int current_offset_ = 0;
    int push_depth_ = 0;
    int label_count_ = 0;
    int string_count_ = 0;
    std::vector<std::pair<std::string, std::string>> strings_;
    Type current_ret_ = ty::Void;
    int ret_slot_off_ = 0;     // frame slot holding the caller's struct result-buffer address

    std::string current_break_label_;
    std::string current_continue_label_;

    std::ostream& emit() { return *cur_; }
    Type type_of(const Expr* e) const;

    void emit_builtins();
    void emit_globals(Globals& globals);
    void emit_rodata();
    void gen_function(FuncDecl& fn);

    void push_scope();
    void pop_scope();
    void allocate_var(const std::string& name, Type type, int bytes = 8, bool is_array = false);
    VarSlot lookup_var(const std::string& name);
    std::string slot_ref(const VarSlot& s) const;

    void emit_load_var(const VarSlot& s);
    void emit_store_var(const VarSlot& s);
    void emit_load(Type elem);        // address in %rax -> value in %rax
    void emit_store(Type elem);       // address in %rdx, value in %rax
    void emit_struct_copy(int size);  // copy struct at %rax to %rdx; result: %rdx in %rax
    void emit_load_rcx(Type elem);    // address in %rdx -> value in %rcx
    void emit_scale(const char* reg, int size);
    void emit_convert(Type from, Type to);
    void emit_to_bool_from_double();
    void gen_double_binary(Tok op);
    void gen_pointer_binary(Tok op, Type lt, Type rt);
    Type emit_binop(Tok op, Type lt, Type rt);   // %rcx = lhs, %rax = rhs -> %rax
    void gen_cond(Expr* e);
    void gen_addr(Expr* e);           // address of an lvalue into %rax
    std::string global_const(Expr* e, Type target);

    void gen_stmt(Stmt* s);
    void gen_expr(Expr* e);
    void gen_block(Block* b);
    void gen_call(Call* call);
    std::string new_label();
    std::string add_string(const std::string& s);
};
