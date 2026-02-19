//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "ast.h"

Expr Expr::number(int v, SourceLoc loc) {
    Expr e;
    e.type = Type::Number;
    e.value = v;
    e.loc = loc;
    return e;
}

Expr Expr::var(const std::string& n, SourceLoc loc) {
    Expr e;
    e.type = Type::Var;
    e.name = n;
    e.loc = loc;
    return e;
}

Expr Expr::binop(TokenType op, Expr lhs, Expr rhs, SourceLoc loc) {
    Expr e;
    e.type = Type::BinOp;
    e.op = op;
    e.left = std::make_unique<Expr>(std::move(lhs));
    e.right = std::make_unique<Expr>(std::move(rhs));
    e.loc = loc;
    return e;
}

Expr Expr::unary(TokenType op, Expr inner, SourceLoc loc) {
    Expr e;
    e.type = Type::UnaryOp;
    e.op = op;
    e.inner = std::make_unique<Expr>(std::move(inner));
    e.loc = loc;
    return e;
}
