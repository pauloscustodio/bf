//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "ast.h"

Expr Expr::number(int v, const SourceLoc& loc) {
    Expr e;
    e.type = ExprType::Number;
    e.value = v;
    e.loc = loc;
    return e;
}

Expr Expr::var(const std::string& n, const SourceLoc& loc) {
    Expr e;
    e.type = ExprType::Var;
    e.name = n;
    e.loc = loc;
    return e;
}

Expr Expr::binop(TokenType op, Expr lhs, Expr rhs, const SourceLoc& loc) {
    Expr e;
    e.type = ExprType::BinOp;
    e.op = op;
    e.left = std::make_unique<Expr>(std::move(lhs));
    e.right = std::make_unique<Expr>(std::move(rhs));
    e.loc = loc;
    return e;
}

Expr Expr::unary(TokenType op, Expr inner, const SourceLoc& loc) {
    Expr e;
    e.type = ExprType::UnaryOp;
    e.op = op;
    e.inner = std::make_unique<Expr>(std::move(inner));
    e.loc = loc;
    return e;
}

Expr Expr::make_array_access(const std::string& n, std::unique_ptr<Expr> index,
                             const SourceLoc& loc) {
    Expr e;
    e.type = ExprType::ArrayAccess;
    e.name = n;
    e.index = std::move(index);
    e.loc = loc;
    return e;
}

PrintElem PrintElem::string(std::string s) {
    return { PrintElemType::String, std::move(s), {}, {} };
}

PrintElem PrintElem::expression(Expr e) {
    return { PrintElemType::Expr, "", std::move(e), {} };
}

PrintElem PrintElem::separator(TokenType t) {
    return { PrintElemType::Expr, "", {}, t };
}
