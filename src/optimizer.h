#pragma once
#include "ast.h"
#include "ast_ext.h"
#include "sema.h"

struct OptStats {
    int folded = 0;       // constant expressions folded
    int simplified = 0;   // algebraic identities and canonicalisations
    int branches = 0;     // constant conditions resolved
    int dead_stmts = 0;   // statements removed
};

// AST-level optimizer. Runs after Sema (it needs expression types) and before Codegen.
class Optimizer {
public:
    explicit Optimizer(Sema& sema) : sema_(sema) {}
    void run(Program& program);
    const OptStats& stats() const { return stats_; }

private:
    Sema& sema_;
    OptStats stats_;

    Type type_of(const Expr* e) const;
    bool replace_int(ExprPtr& slot, long long v);
    bool replace_bool(ExprPtr& slot, bool v);
    bool replace_char(ExprPtr& slot, char v);
    bool replace_double(ExprPtr& slot, double v);

    void fold(ExprPtr& e);
    void fold_binary_node(ExprPtr& e, Binary* b);
    void opt_block(Block* b);
    void opt_stmt(StmtPtr& s);
};