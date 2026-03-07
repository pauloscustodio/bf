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

enum class ValueType {
    Int,
    String
};

enum class ExprType {
    Number,
    Var,
    BinOp,      // arithmetic only, operands are numeric, result is numeric
    RelOp,      // relational, operands can be Int or String,
    UnaryOp,    // unary operations, operands are numeric, result is numeric
    ArrayAccess,    // A(i)
    StringLiteral,  // "hello"
    Concat,         // B$ & C$
    Call,           // LEFT$(...), MID$(...), RIGHT$(...), ...
};

struct Expr {
    ExprType expr_type = ExprType::Number;   // filled in by parser
    ValueType value_type = ValueType::Int;
    ValueType operand_type = ValueType::Int; // for RelOp, type of operands
    SourceLoc loc;

    // For Number
    int int_value = 0;

    // For Var
    std::string name;

    // For StringLiteral
    std::string string_value;
    int string_size = 0;    // filled in by semantic pass

    // For operators
    TokenType op = TokenType::EndOfFile;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    std::unique_ptr<Expr> inner;

    // For ArrayAccess
    std::unique_ptr<Expr> index;
    bool is_string_array = false;  // filled by semantic pass

    // For function call
    TokenType func;
    std::vector<std::unique_ptr<Expr>> args;

    static Expr number(int v, const SourceLoc& loc);
    static Expr var(const std::string& n, const SourceLoc& loc);
    static Expr binop(TokenType op, Expr lhs, Expr rhs, const SourceLoc& loc);
    static Expr unary(TokenType op, Expr inner, const SourceLoc& loc);
    static Expr relop(TokenType op, Expr lhs, Expr rhs, ValueType operand_type,
                      const SourceLoc& loc);
    static Expr array_access(const std::string& n, std::unique_ptr<Expr> index,
                             const SourceLoc& loc);
    static Expr string_literal(std::string s, const SourceLoc& loc);
    static Expr concat(Expr lhs, Expr rhs, const SourceLoc& loc);
    static Expr call(Token func_tok, std::vector<std::unique_ptr<Expr>> args,
                     const SourceLoc& loc);
};

struct Stmt;

struct StmtList {
    std::vector<std::unique_ptr<Stmt>> statements;
};

enum class LetType {
    Normal,     // LET A = 5
    Array,      // LET A(i) = 5
    String,     // LET A$ = "hello"
};

struct LetStmt {
    std::string var;
    LetType type = LetType::Normal;
    std::unique_ptr<Expr> index;    // for A(i)
    Expr expr;                      // RHS
};

struct DimElem {
    std::string var;
    std::unique_ptr<Expr> size_expr;
};

struct DimStmt {
    std::vector<DimElem> elems;
};

struct InputStmt {
    std::vector<std::string> vars;
};

enum class PrintElemType {
    String,     // TODO: can be removed, Expr can represent string literals
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
