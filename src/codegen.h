#pragma once
#include "ast.h"
#include "sema.h"
#include "types.h"
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct VarSlot {
    int off;         // distance below %rbp of the slot's lowest address
    bool is_array;
    Type type;       // for arrays: pointer to element type
};

class Codegen {
public:
    Codegen(std::ostream& out, const Sema& sema);
    void generate(Program& program);

private:
    std::ostream& out_;
    std::ostream* cur_;
    const Sema& sema_;
    std::vector<std::unordered_map<std::string, VarSlot>> scopes_;
    int current_offset_ = 0;
    int push_depth_ = 0;
    int label_count_ = 0;
    int string_count_ = 0;
    std::vector<std::pair<std::string, std::string>> strings_;
    Type current_ret_ = ty::Void;

    std::string current_break_label_;
    std::string current_continue_label_;

    std::ostream& emit() { return *cur_; }
    Type type_of(const Expr* e) const;

    void emit_builtins();
    void emit_rodata();
    void gen_function(FuncDecl& fn);

    void push_scope();
    void pop_scope();
    void allocate_var(const std::string& name, Type type, int bytes = 8, bool is_array = false);
    VarSlot lookup_var(const std::string& name);

    void emit_load_var(const VarSlot& s);
    void emit_load(Type elem);     // address in %rax -> value in %rax
    void emit_store(Type elem);    // address in %rdx, value in %rax
    void emit_scale(const char* reg, int size);
    void emit_convert(Type from, Type to);
    void emit_to_bool_from_double();
    void gen_double_binary(Tok op);
    void gen_pointer_binary(Tok op, Type lt, Type rt);
    void gen_cond(Expr* e);
    void gen_addr(Expr* e);        // address of an lvalue into %rax

    void gen_stmt(Stmt* s);
    void gen_expr(Expr* e);
    void gen_block(Block* b);
    void gen_call(Call* call);
    std::string new_label();
    std::string add_string(const std::string& s);
};