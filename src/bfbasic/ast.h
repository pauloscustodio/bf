//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "errors.h"
#include "lexer.h"
#include <memory>
#include <string>
#include <vector>

enum class ExprType {
    Number, Var, BinOp, UnaryOp, ArrayAccess,
};

struct Expr {
    ExprType type;
    SourceLoc loc;

    // For Number
    int value = 0;

    // For Var
    std::string name;

    // For operators
    TokenType op = TokenType::EndOfFile;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    std::unique_ptr<Expr> inner;

    // For ArrayAccess
    std::unique_ptr<Expr> index;

    static Expr number(int v, const SourceLoc& loc);
    static Expr var(const std::string& n, const SourceLoc& loc);
    static Expr binop(TokenType op, Expr lhs, Expr rhs, const SourceLoc& loc);
    static Expr unary(TokenType op, Expr inner, const SourceLoc& loc);
    static Expr make_array_access(const std::string& n, std::unique_ptr<Expr> index,
                                  const SourceLoc& loc);
};

enum class PrintElemType {
    String,
    Expr,
    Separator,
};

struct PrintElem {
    PrintElemType type;
    std::string text;   // for strings
    Expr expr;          // for expressions
    TokenType sep;      // for separators: Semicolon or Comma

    static PrintElem string(std::string s);
    static PrintElem expression(Expr e);
    static PrintElem separator(TokenType t);
};

struct PrintStmt {
    std::vector<PrintElem> elems;
};

struct Stmt;

struct StmtList {
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct IfStmt {
    Expr condition;
    StmtList then_block;
    StmtList else_block;
};

struct WhileStmt {
    Expr condition;
    StmtList body;
};

struct ForStmt {
    std::string var;      // loop variable
    Expr start_expr;
    Expr end_expr;
    Expr step_expr;       // optional; default = 1
    StmtList body;
};

struct LetStmt {
    bool is_array = false;
    std::string var;
    std::unique_ptr<Expr> index;   // only if is_array == true
    Expr expr;
};

struct InputStmt {
    std::vector<std::string> vars;
};

struct DimStmt {
    std::string var;
    std::unique_ptr<Expr> size_expr;
};

enum class StmtType {
    Let,
    Dim,
    Input,
    Print,
    If,
    While,
    For,
};

struct Stmt {
    StmtType type;
    SourceLoc loc;

    std::unique_ptr<LetStmt> let_stmt;          // LET
    std::unique_ptr<DimStmt> dim_stmt;          // DIM
    std::unique_ptr<InputStmt> input_stmt;      // INPUT
    std::unique_ptr <PrintStmt> print_stmt;     // PRINT
    std::unique_ptr<IfStmt> if_stmt;            // IF
    std::unique_ptr<WhileStmt> while_stmt;      // WHILE
    std::unique_ptr<ForStmt> for_stmt;          // FOR
};

using Program = StmtList;
