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
    const Token& peek_next() const;
    bool eof() const;
    const Token& advance();
    bool match(TokenType t) const;
    [[noreturn]] void error_here(const std::string& msg) const;
    const Token& expect(TokenType t, const std::string& msg);

    // --- Statements ----------------------------------------------------------

    Stmt parse_statement();
    Stmt parse_let();
    Stmt parse_let_without_keyword();
    Stmt parse_input();
    Stmt parse_print();
    PrintElem parse_print_elems();
    void consume_end_of_statement();

    // --- Expressions ---------------------------------------------------------

    bool starts_expression(const Token& t) const;
    Expr parse_expr();
    Expr parse_or();
    Expr parse_xor();
    Expr parse_and();
    Expr parse_relational();
    Expr parse_shift();
    Expr parse_add();
    Expr parse_mul();
    Expr parse_unary();
    Expr parse_power();
    Expr parse_primary();
};
