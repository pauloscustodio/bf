//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "lexer.h"
#include <memory>
#include <string>
#include <vector>

struct SourceLoc {
    int line;
};

struct Expr {
    enum class Type { Number, Var, BinOp, UnaryOp } type;

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

    static Expr number(int v, SourceLoc loc);
    static Expr var(const std::string& n, SourceLoc loc);
    static Expr binop(TokenType op, Expr lhs, Expr rhs, SourceLoc loc);
    static Expr unary(TokenType op, Expr inner, SourceLoc loc);
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

struct StmtPrint {
    std::vector<PrintElem> elems;
};

struct Stmt;

struct StmtList {
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct StmtIf {
    Expr condition;
    StmtList then_block;
    StmtList else_block;
};

enum class StmtType {
    Let,
    Input,
    Print,
    If,
};

struct Stmt {
    StmtType type;
    SourceLoc loc;

    // LET and INPUT
    std::vector<std::string> vars;
    std::unique_ptr<Expr> expr; // only for LET

    // PRINT ...
    StmtPrint print;

    // IF ...
    std::unique_ptr<StmtIf> if_stmt;
};

using Program = StmtList;
