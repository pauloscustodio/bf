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

        parse_statement_list_on_line(prog.statements);
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

bool Parser::consume_any(std::initializer_list<TokenType> types) {
    if (match_any(types)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(TokenType t) const {
    return peek().type == t;
}

bool Parser::match_any(std::initializer_list<TokenType> types) const {
    TokenType t = peek().type;
    for (auto tt : types) {
        if (t == tt) {
            return true;
        }
    }
    return false;
}


[[noreturn]]
void Parser::error_here(const std::string& msg) const {
    const Token& t = peek();
    std::cerr << "Parse error at line " << t.line << ": " << msg << "\n";
    exit(EXIT_FAILURE);
}

const Token& Parser::expect(TokenType t, const std::string& msg) {
    if (!match(t)) {
        error_here(msg);
    }
    return advance();
}

bool Parser::starts_statement(const Token& t) const {
    switch (t.type) {

    case TokenType::KeywordLet:
    case TokenType::KeywordInput:
    case TokenType::KeywordPrint:
    case TokenType::KeywordIf:
        return true;

    case TokenType::Identifier:
        if(peek_next().type == TokenType::Equal) {
            return true; // LET without keyword
        }
        return false;

    default:
        return false;
    }
}

Stmt Parser::parse_single_statement() {
    if (match(TokenType::KeywordLet)) {
        return parse_let();
    }
    if (match(TokenType::KeywordInput)) {
        return parse_input();
    }
    if (match(TokenType::KeywordPrint)) {
        return parse_print();
    }
    if (match(TokenType::KeywordIf)) {
        return parse_if();
    }

    // optional LET
    if (match(TokenType::Identifier) && peek_next().type == TokenType::Equal) {
        return parse_let_without_keyword();
    }

    error_here("Expected LET, INPUT, PRINT or IF");
}

void Parser::parse_statement_list_on_line(
    std::vector<std::unique_ptr<Stmt>>& out) {
    while (starts_statement(peek()) || match(TokenType::Colon)) {
        // sequence of colons
        if (match(TokenType::Colon)) {
            advance();
            continue;
        }

        // one statement
        out.push_back(std::make_unique<Stmt>(parse_single_statement()));

        // optional colon to keep going on the same line
        if (!match(TokenType::Colon)) {
            break; // end of statement list on this line will be checked by caller
        }
    }
}

StmtList Parser::parse_statement_list_until(std::initializer_list<TokenType> terminators) {
    StmtList list;

    while (!match_any(terminators)) {
        // Skip blank separators
        if (consume_any({ TokenType::Newline, TokenType::Colon })) {
            continue;
        }

        parse_statement_list_on_line(list.statements);
        consume_end_of_statement();
    }

    return list;
}

Stmt Parser::parse_let() {
    Token kw = advance(); // LET

    Token id = expect(TokenType::Identifier, "Expected variable name after LET");
    expect(TokenType::Equal, "Expected '=' after variable name");

    Expr e = parse_expr();

    Stmt s;
    s.type = StmtType::Let;
    s.loc.line = kw.line;
    s.vars = { id.text };
    s.expr = std::make_unique<Expr>(std::move(e));

    return s;
}

Stmt Parser::parse_let_without_keyword() {
    Token id = expect(TokenType::Identifier, "Expected variable name");
    expect(TokenType::Equal, "Expected '=' after variable name");

    Expr e = parse_expr();

    Stmt s;
    s.type = StmtType::Let;
    s.loc.line = id.line;
    s.vars = { id.text };
    s.expr = std::make_unique<Expr>(std::move(e));

    return s;
}

Stmt Parser::parse_input() {
    Token kw = advance(); // INPUT
    Token id = expect(TokenType::Identifier, "Expected variable name after INPUT");

    Stmt s;
    s.type = StmtType::Input;
    s.loc.line = kw.line;
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
    s.type = StmtType::Print;
    s.loc.line = kw.line;

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

Stmt Parser::parse_if() {
    Token kw = advance(); // IF

    Stmt s;
    s.type = StmtType::If;
    s.loc.line = kw.line;
    s.if_stmt = std::make_unique<StmtIf>();

    // Parse condition
    Expr condition = parse_expr();

    expect(TokenType::KeywordThen, "Expected THEN");

    // THEN followed by newline -> multi-line IF
    if (match(TokenType::Newline)) {
        advance(); // consume newline
        return parse_multiline_if(std::move(condition));
    }

    // single-line IF
    s.if_stmt->condition = std::move(condition);

    // Parse THEN inline block
    parse_inline_stmt_list(s.if_stmt->then_block);

    // Optional ELSE
    if (match(TokenType::KeywordElse)) {
        advance(); // consume ELSE

        // ELSE followed by newline -> multi-line ELSE (not yet supported)
        if (match(TokenType::Newline) || match(TokenType::EndOfFile)) {
            error_here("Multi-line ELSE blocks not yet supported");
        }

        parse_inline_stmt_list(s.if_stmt->else_block);
    }

    return s;
}

void Parser::parse_inline_stmt_list(StmtList& out) {
    // Skip any number of leading colons
    while (match(TokenType::Colon)) {
        advance();
    }

    // If we hit newline/EOF here -> THEN/ELSE with no statements -> error
    if (match(TokenType::Newline) || match(TokenType::EndOfFile)) {
        error_here("Expected statement after THEN/ELSE");
    }

    // List of statements on the same line, separated by colons
    parse_statement_list_on_line(out.statements);
}

Stmt Parser::parse_multiline_if(Expr condition) {
    auto if_stmt = std::make_unique<StmtIf>();
    if_stmt->condition = std::move(condition);

    // Parse THEN block until ELSE or ENDIF
    if_stmt->then_block = parse_block_until(
    { TokenType::KeywordElse, TokenType::KeywordEndIf }, "ENDIF");

    // Optional ELSE
    if (match(TokenType::KeywordElse)) {
        advance(); // consume ELSE
        expect(TokenType::Newline, "Expected newline after ELSE");
        if_stmt->else_block = parse_block_until({ TokenType::KeywordEndIf }, "ENDIF");
    }

    // ENDIF
    expect(TokenType::KeywordEndIf, "Expected ENDIF");

    // ENDIF must be followed by newline
    if (!match(TokenType::Newline)) {
        error_here("Expected newline after ENDIF");
    }

    Stmt s;
    s.type = StmtType::If;
    s.if_stmt = std::move(if_stmt);
    return s;
}

StmtList Parser::parse_block_until(
    std::initializer_list<TokenType> terminators,
    const std::string& terminator_name) {
    StmtList list;

    while (true) {
        // If we reached a terminator, stop
        if (match_any(terminators)) {
            break;
        }

        // If we reached EOF before a terminator → missing ENDIF
        if (match(TokenType::EndOfFile)) {
            error_here("Missing " + terminator_name);
        }

        // Skip blank lines
        if (match(TokenType::Newline)) {
            advance();
            continue;
        }

        // Parse all statements on this line
        parse_statement_list_on_line(list.statements);

        // After statements, we expect either newline or EOF
        if (match(TokenType::Newline)) {
            advance();
        }
        else if (match(TokenType::EndOfFile)) {
            // EOF before terminator → missing ENDIF
            error_here("Missing " + terminator_name);
        }
        else {
            error_here("Expected newline after statements");
        }
    }

    return list;
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
        { op.line });
    }

    return left;
}

Expr Parser::parse_xor() {
    Expr left = parse_and();

    while (match(TokenType::KeywordXor)) {
        Token op = advance();
        Expr right = parse_and();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line });
    }

    return left;
}

Expr Parser::parse_and() {
    Expr left = parse_relational();

    while (match(TokenType::KeywordAnd)) {
        Token op = advance();
        Expr right = parse_relational();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line });
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
        { op.line });
    }

    return left;
}

Expr Parser::parse_shift() {
    Expr left = parse_add();

    while (match(TokenType::KeywordShl) || match(TokenType::KeywordShr)) {
        Token op = advance();
        Expr right = parse_add();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line });
    }

    return left;
}

Expr Parser::parse_add() {
    Expr left = parse_mul();

    while (match(TokenType::Plus) || match(TokenType::Minus)) {
        Token op = advance();
        Expr right = parse_mul();
        left = Expr::binop(op.type, std::move(left), std::move(right),
        { op.line });
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
        { op.line });
    }

    return left;
}

Expr Parser::parse_unary() {
    if (match(TokenType::Plus) || match(TokenType::Minus)) {
        Token op = advance();
        Expr inner = parse_unary();
        return Expr::unary(op.type, std::move(inner),
        { op.line });
    }
    if (match(TokenType::KeywordNot)) {
        Token op = advance();
        Expr inner = parse_unary();
        return Expr::unary(op.type, std::move(inner),
        { op.line });
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
        { op.line });
    }
    return left;
}

Expr Parser::parse_primary() {
    Token t = peek();

    if (match(TokenType::Number)) {
        advance();
        return Expr::number(t.value, { t.line });
    }

    if (match(TokenType::Identifier)) {
        advance();
        return Expr::var(t.text, { t.line });
    }

    if (match(TokenType::LParen)) {
        advance();
        Expr e = parse_expr();
        expect(TokenType::RParen, "Expected ')'");
        return e;
    }

    error_here("Expected number, variable, unary operator, or '('");
}
