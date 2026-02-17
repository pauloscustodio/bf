//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <string>
#include <vector>

struct SourceLoc {
    int line;
    int column;
};

struct Expr {
    enum class Type { Number, Var, BinOp } type;

    SourceLoc loc;

    // For Number
    int value = 0;

    // For Var
    std::string name;

    // For BinOp
    char op = 0; // '+', '-', '*', '/'
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    static Expr number(int v, SourceLoc loc);
    static Expr var(const std::string& n, SourceLoc loc);
    static Expr binop(char op, Expr lhs, Expr rhs, SourceLoc loc);
};

struct Stmt {
    enum class Type { Let, Input, Print } type;

    SourceLoc loc;

    // LET v = expr
    std::string var;
    std::unique_ptr<Expr> expr;

    // INPUT v
    // PRINT v
    // (both use var)
};

struct Program {
    std::vector<Stmt> statements;
};
