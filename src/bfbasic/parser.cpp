//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), pos(0) {
}

Program Parser::parse_program() {
    Program prog;

    while (!match(TokenType::EndOfFile)) {

        // Skip blank lines or stray separators
        if (match(TokenType::Newline) || match(TokenType::Colon)) {
            advance();
            continue;
        }

        Stmt s = parse_statement();
        prog.statements.push_back(std::move(s));

        consume_end_of_statement();
    }

    return prog;
}

const Token& Parser::peek() const {
    if (pos < tokens.size()) {
        return tokens[pos];
    }
    return tokens.back(); // always EOF
}

const Token& Parser::peek_next() const {
    if (pos + 1 < tokens.size()) {
        return tokens[pos + 1];
    }
    return tokens.back(); // always EOF
}

bool Parser::eof() const {
    return peek().type == TokenType::EndOfFile;
}

const Token& Parser::advance() {
    return tokens[pos++];
}

bool Parser::match(TokenType t) const {
    return peek().type == t;
}


[[noreturn]]
void Parser::error_here(const std::string& msg) const {
    const Token& t = peek();
    std::cerr << "Parse error at line " << t.line
              << ", column " << t.column << ": " << msg << "\n";
    exit(EXIT_FAILURE);
}

const Token& Parser::expect(TokenType t, const std::string& msg) {
    if (!match(t)) {
        error_here(msg);
    }
    return advance();
}

Stmt Parser::parse_statement() {
    if (match(TokenType::KeywordLet)) {
        return parse_let();
    }
    if (match(TokenType::KeywordInput)) {
        return parse_input();
    }
    if (match(TokenType::KeywordPrint)) {
        return parse_print();
    }

    // optional LET
    if (match(TokenType::Identifier) && peek_next().type == TokenType::Equal) {
        return parse_let_without_keyword();
    }

    error_here("Expected LET, INPUT, or PRINT");
}

Stmt Parser::parse_let() {
    Token kw = advance(); // LET

    Token id = expect(TokenType::Identifier, "Expected variable name after LET");
    expect(TokenType::Equal, "Expected '=' after variable name");

    Expr e = parse_expr();

    Stmt s;
    s.type = Stmt::Type::Let;
    s.loc = { kw.line, kw.column };
    s.vars = { id.text };
    s.expr = std::make_unique<Expr>(std::move(e));

    return s;
}

Stmt Parser::parse_let_without_keyword() {
    Token id = expect(TokenType::Identifier, "Expected variable name");
    expect(TokenType::Equal, "Expected '=' after variable name");

    Expr e = parse_expr();

    Stmt s;
    s.type = Stmt::Type::Let;
    s.loc = { id.line, id.column };
    s.vars = { id.text };
    s.expr = std::make_unique<Expr>(std::move(e));

    return s;
}

Stmt Parser::parse_input() {
    Token kw = advance(); // INPUT
    Token id = expect(TokenType::Identifier, "Expected variable name after INPUT");

    Stmt s;
    s.type = Stmt::Type::Input;
    s.loc = { kw.line, kw.column };
    s.vars = { id.text };

    while (match(TokenType::Comma)) {
        advance();
        id = expect(TokenType::Identifier, "Expected variable name after ,");
        s.vars.push_back(id.text);
    }

    return s;
}

Stmt Parser::parse_print() {
    Token kw = advance(); // PRINT

    Stmt s;
    s.type = Stmt::Type::Print;
    s.loc = { kw.line, kw.column };

    while (true) {
        // 1. Separator?
        if (match(TokenType::Semicolon) || match(TokenType::Comma)) {
            Token sep = advance();
            PrintElem e;
            e.type = PrintElemType::Separator;
            e.sep = sep.type;
            s.print.elems.push_back(std::move(e));
            continue;
        }

        // 2. String literal?
        if (match(TokenType::StringLiteral)) {
            Token t = advance();
            PrintElem e;
            e.type = PrintElemType::String;
            e.text = t.text;
            s.print.elems.push_back(std::move(e));
            continue;
        }

        // 3. Expression?
        if (starts_expression(peek())) {
            Expr ex = parse_expr();
            PrintElem e;
            e.type = PrintElemType::Expr;
            e.expr = std::move(ex);
            s.print.elems.push_back(std::move(e));
            continue;
        }

        // 4. End of PRINT
        break;
    }

    return s;
}

PrintElem Parser::parse_print_elems() {
    Token t = peek();

    if (match(TokenType::StringLiteral)) {
        advance();
        return PrintElem::string(t.text);
    }

    // Otherwise, it's an expression
    Expr e = parse_expr();
    return PrintElem::expression(std::move(e));
}

void Parser::consume_end_of_statement() {

    // Allow any number of Newline or Colon tokens
    if (match(TokenType::Newline) || match(TokenType::Colon)) {
        while (match(TokenType::Newline) || match(TokenType::Colon)) {
            advance();
        }
        return;
    }

    // Otherwise, require EOF
    if (match(TokenType::EndOfFile)) {
        return;
    }

    error_here("Unexpected token after statement");
}

bool Parser::starts_expression(const Token& t) const {
    switch (t.type) {

    case TokenType::Number:
    case TokenType::Identifier:
    case TokenType::LParen:
        return true;

    case TokenType::Plus:
    case TokenType::Minus:
    case TokenType::KeywordNot:
        // unary operators
        return true;

    default:
        return false;
    }
}

Expr Parser::parse_expr() {
    return parse_or();
}

Expr Parser::parse_or() {
    Expr left = parse_xor();

    while (match(TokenType::KeywordOr)) {
        Token op = advance();
        Expr right = parse_xor();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line, op.column });
    }

    return left;
}

Expr Parser::parse_xor() {
    Expr left = parse_and();

    while (match(TokenType::KeywordXor)) {
        Token op = advance();
        Expr right = parse_and();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line, op.column });
    }

    return left;
}

Expr Parser::parse_and() {
    Expr left = parse_relational();

    while (match(TokenType::KeywordAnd)) {
        Token op = advance();
        Expr right = parse_relational();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line, op.column });
    }

    return left;
}

Expr Parser::parse_relational() {
    Expr left = parse_shift();

    while (match(TokenType::Equal) ||
            match(TokenType::NotEqual) ||
            match(TokenType::Less) ||
            match(TokenType::LessEqual) ||
            match(TokenType::Greater) ||
            match(TokenType::GreaterEqual)) {

        Token op = advance();
        Expr right = parse_shift();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line, op.column });
    }

    return left;
}

Expr Parser::parse_shift() {
    Expr left = parse_add();

    while (match(TokenType::KeywordShl) || match(TokenType::KeywordShr)) {
        Token op = advance();
        Expr right = parse_add();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line, op.column });
    }

    return left;
}

Expr Parser::parse_add() {
    Expr left = parse_mul();

    while (match(TokenType::Plus) || match(TokenType::Minus)) {
        Token op = advance();
        Expr right = parse_mul();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line, op.column });
    }

    return left;
}

Expr Parser::parse_mul() {
    Expr left = parse_unary();

    while (match(TokenType::Star) ||
            match(TokenType::Slash) ||   // includes '\'
            match(TokenType::KeywordMod)) {
        Token op = advance();
        Expr right = parse_unary();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line, op.column });
    }

    return left;
}

Expr Parser::parse_unary() {
    if (match(TokenType::Plus) || match(TokenType::Minus)) {
        Token op = advance();
        Expr inner = parse_unary();
        return Expr::unary(op.type, std::move(inner),
        { op.line, op.column });
    }
    if (match(TokenType::KeywordNot)) {
        Token op = advance();
        Expr inner = parse_unary();
        return Expr::unary(op.type, std::move(inner),
        { op.line, op.column });
    }
    return parse_power();
}

Expr Parser::parse_power() {
    Expr left = parse_primary();
    if (match(TokenType::Caret)) {
        Token op = advance();
        // right-associative; no unary sign allowed directly on the exponent
        Expr right = parse_power();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line, op.column });
    }
    return left;
}

Expr Parser::parse_primary() {
    Token t = peek();

    if (match(TokenType::Number)) {
        advance();
        return Expr::number(t.value, { t.line, t.column });
    }

    if (match(TokenType::Identifier)) {
        advance();
        return Expr::var(t.text, { t.line, t.column });
    }

    if (match(TokenType::LParen)) {
        advance();
        Expr e = parse_expr();
        expect(TokenType::RParen, "Expected ')'");
        return e;
    }

    error_here("Expected number, variable, unary operator, or '('");
}
