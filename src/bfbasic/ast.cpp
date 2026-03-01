//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "ast.h"
#include "lexer.h"

Expr Expr::number(int v, const SourceLoc& loc) {
    Expr e;
    e.expr_type = ExprType::Number;
    e.value_type = ValueType::Int;
    e.int_value = v;
    e.loc = loc;
    return e;
}

Expr Expr::var(const std::string& n, const SourceLoc& loc) {
    Expr e;
    e.expr_type = ExprType::Var;
    e.value_type = is_string_var(n) ? ValueType::String : ValueType::Int;
    e.name = n;
    e.loc = loc;
    return e;
}

Expr Expr::binop(TokenType op, Expr lhs, Expr rhs, const SourceLoc& loc) {
    Expr e;
    e.expr_type = ExprType::BinOp;
    e.value_type = ValueType::Int;   // will be fixed by semantic pass if needed
    e.op = op;
    e.left = std::make_unique<Expr>(std::move(lhs));
    e.right = std::make_unique<Expr>(std::move(rhs));
    e.loc = loc;
    return e;
}

Expr Expr::unary(TokenType op, Expr inner, const SourceLoc& loc) {
    Expr e;
    e.expr_type = ExprType::UnaryOp;
    e.value_type = ValueType::Int;   // will be fixed by semantic pass if needed
    e.op = op;
    e.inner = std::make_unique<Expr>(std::move(inner));
    e.loc = loc;
    return e;
}

Expr Expr::array_access(const std::string& n, std::unique_ptr<Expr> index,
                        const SourceLoc& loc) {
    Expr e;
    e.expr_type = ExprType::ArrayAccess;
    e.value_type = is_string_var(n) ? ValueType::String : ValueType::Int;
    e.name = n;
    e.index = std::move(index);
    e.loc = loc;
    return e;
}

Expr Expr::string_literal(std::string s, const SourceLoc& loc) {
    Expr e;
    e.expr_type = ExprType::StringLiteral;
    e.value_type = ValueType::String;
    e.string_value = std::move(s);
    e.loc = loc;
    return e;
}

Expr Expr::concat(Expr lhs, Expr rhs, const SourceLoc& loc) {
    Expr e;
    e.expr_type = ExprType::Concat;
    e.value_type = ValueType::String;
    e.left = std::make_unique<Expr>(std::move(lhs));
    e.right = std::make_unique<Expr>(std::move(rhs));
    e.loc = loc;
    return e;
}

Expr Expr::call(std::string fname, std::vector<std::unique_ptr<Expr>> args,
                const SourceLoc& loc) {
    Expr e;
    e.expr_type = ExprType::Call;
    e.value_type = is_string_var(fname) ? ValueType::String : ValueType::Int;
    e.func_name = std::move(fname);
    e.args = std::move(args);
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
