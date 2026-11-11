#pragma once
#include "ast.h"
#include "ast_ext.h"
#include "sema.h"
#include <cstddef>
#include <ostream>

// Prints the AST as an indented tree. When `sema` is given, expression types are shown.
// The first `skip_functions` functions (the built-in prelude) are not printed.
void dump_program(const Program& prog, const Globals& globals, std::ostream& os,
                  const Sema* sema, std::size_t skip_functions);