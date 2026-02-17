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
        if (match(TokenType::Newline)) {
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
    s.var = id.text;
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
    s.var = id.text;
    s.expr = std::make_unique<Expr>(std::move(e));

    return s;
}

Stmt Parser::parse_input() {
    Token kw = advance(); // INPUT

    Token id = expect(TokenType::Identifier, "Expected variable name after INPUT");

    Stmt s;
    s.type = Stmt::Type::Input;
    s.loc = { kw.line, kw.column };
    s.var = id.text;

    return s;
}

Stmt Parser::parse_print() {
    Token kw = advance(); // PRINT

    Token id = expect(TokenType::Identifier, "Expected variable name after PRINT");

    Stmt s;
    s.type = Stmt::Type::Print;
    s.loc = { kw.line, kw.column };
    s.var = id.text;

    return s;
}

void Parser::consume_end_of_statement() {
    // Allow multiple newlines (blank lines)
    if (match(TokenType::Newline)) {
        while (match(TokenType::Newline)) {
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

// expr ::= term { ("+"|"-"|"*"|"/") term }
Expr Parser::parse_expr() {
    Expr left = parse_term();

    while (match(TokenType::Plus) ||
            match(TokenType::Minus) ||
            match(TokenType::Star) ||
            match(TokenType::Slash)) {

        Token op = advance();
        Expr right = parse_term();

        left = Expr::binop(op.text[0], std::move(left), std::move(right),
        { op.line, op.column });
    }

    return left;
}

// term ::= NUMBER | IDENT
Expr Parser::parse_term() {
    Token t = peek();

    if (match(TokenType::Number)) {
        advance();
        return Expr::number(t.value, { t.line, t.column });
    }

    if (match(TokenType::Identifier)) {
        advance();
        return Expr::var(t.text, { t.line, t.column });
    }

    error_here("Expected number or variable");
}
