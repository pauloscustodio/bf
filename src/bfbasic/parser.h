//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "ast.h"
#include "lexer.h"
#include <vector>

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    Program parse_program();

private:
    const std::vector<Token>& tokens;
    size_t pos = 0;

    // --- Utility -------------------------------------------------------------

    const Token& peek() const;
    bool eof() const;
    const Token& advance();
    bool match(TokenType t) const;
    [[noreturn]] void error_here(const std::string& msg) const;
    const Token& expect(TokenType t, const std::string& msg);

    // --- Statements ----------------------------------------------------------

    Stmt parse_statement();
    Stmt parse_let();
    Stmt parse_input();
    Stmt parse_print();
    void consume_end_of_statement();

    // --- Expressions ---------------------------------------------------------

    Expr parse_expr();
    Expr parse_term();
};
