#pragma once
#include <string>

struct PeepStats {
    int removed_lines = 0;   // net lines removed from the assembly
    int push_pop = 0;        // push/pop pairs rewritten as register moves
    int branches = 0;        // compare + setcc + test + jcc sequences fused
    int immediates = 0;      // register operands replaced by immediates
    int dead = 0;            // unreachable instructions removed
};

// Peephole optimizer over AT&T x86-64 assembly text produced by Codegen.
std::string peephole(const std::string& text, PeepStats& stats);