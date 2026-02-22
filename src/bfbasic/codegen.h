//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "ast.h"
#include "symbols.h"

class CodeGen {
public:
    CodeGen(SymbolTable& sym);
    std::string generate(const Program& prog);

private:
    SymbolTable& sym;
    int temp_counter;
    std::string out;

    void emit(const std::string& line);
    std::string alloc_temp16();
    void free_temp16(const std::string& name);
    std::vector<std::string> sorted_variable_names() const;

    // --- prelude / postlude --------------------------------------------------

    void emit_prelude();
    void emit_postlude();
    void emit_var_allocs();

    // --- statements ----------------------------------------------------------

    void emit_stmt(const Stmt& s);
    void emit_input(const Stmt& s);
    void emit_print(const Stmt& s);
    std::string escape(const std::string& s);
    void emit_let(const Stmt& s);
    void emit_if(const Stmt& s);
    void emit_while(const Stmt& s);

    // --- expressions ---------------------------------------------------------

    void emit_expr(const Expr& e, const std::string& target);
    void emit_unary(const Expr& e, const std::string& target);
    void emit_binary(const Expr& e, const std::string& target);
};
