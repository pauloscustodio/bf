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
    error_at(t.loc, msg);
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
    case TokenType::KeywordWhile:
    case TokenType::KeywordFor:
    case TokenType::KeywordDim:
        return true;

    case TokenType::Identifier:
        if(peek_next().type == TokenType::Equal) {
            return true; // LET without keyword: A = expr
        }
        if(peek_next().type == TokenType::LParen) {
            return true; // Array assignment: A(expr) = expr
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
    if (match(TokenType::KeywordWhile)) {
        return parse_while();
    }
    if (match(TokenType::KeywordFor)) {
        return parse_for();
    }
    if (match(TokenType::KeywordDim)) {
        return parse_dim();
    }

    // optional LET: both A = expr and A(expr) = expr
    if (match(TokenType::Identifier) &&
            (peek_next().type == TokenType::Equal || peek_next().type == TokenType::LParen)) {
        return parse_let_without_keyword();
    }

    error_here("Expected LET, INPUT, PRINT, IF, WHILE, FOR or DIM");
}

void Parser::parse_statement_list_on_line(
    std::vector<std::unique_ptr<Stmt>>& out) {

    bool parsed_any = false;

    while (starts_statement(peek()) || match(TokenType::Colon)) {
        // sequence of colons
        if (match(TokenType::Colon)) {
            advance();
            continue;
        }

        // one statement
        out.push_back(std::make_unique<Stmt>(parse_single_statement()));
        parsed_any = true;

        // optional colon to keep going on the same line
        if (!match(TokenType::Colon)) {
            break; // end of statement list on this line will be checked by caller
        }
    }

    // If we didn't parse any statements, force a call to get proper error
    if (!parsed_any) {
        parse_single_statement(); // Will error with specific message
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

Stmt Parser::parse_let_common(const SourceLoc& loc) {
    Token id = expect(TokenType::Identifier, "Expected variable name after LET");
    bool is_string = is_string_var(id.text);

    // LET A(i)=expr
    bool is_array = false;
    std::unique_ptr<Expr> index;
    if (match(TokenType::LParen)) {
        if(is_string) {
            error_at(id.loc, "string variable '" + id.text + "' cannot be indexed");
        }
        advance();
        index = std::make_unique<Expr>(parse_expr());
        expect(TokenType::RParen, "Expected ')'");
        is_array = true;
    }

    expect(TokenType::Equal, "Expected '=' after variable name");

    Expr e = parse_expr();

    Stmt s;
    s.type = StmtType::Let;
    s.loc = loc;
    s.let_stmt = std::make_unique<LetStmt>();
    s.let_stmt->var = id.text;
    s.let_stmt->type = is_array ? LetType::Array
                       : is_string ? LetType::String : LetType::Normal;
    s.let_stmt->index = std::move(index);
    s.let_stmt->expr = std::move(e);

    return s;
}

Stmt Parser::parse_let() {
    Token kw = advance(); // LET
    return parse_let_common(kw.loc);
}

Stmt Parser::parse_let_without_keyword() {
    Token id = peek();
    return parse_let_common(id.loc);
}

Stmt Parser::parse_input() {
    Token kw = advance(); // INPUT
    Token id = expect(TokenType::Identifier, "Expected variable name after INPUT");

    Stmt s;
    s.type = StmtType::Input;
    s.loc = kw.loc;
    s.input_stmt = std::make_unique<InputStmt>();
    s.input_stmt->vars = { id.text };

    while (match(TokenType::Comma)) {
        advance();
        id = expect(TokenType::Identifier, "Expected variable name after ,");
        s.input_stmt->vars.push_back(id.text);
    }

    return s;
}

Stmt Parser::parse_print() {
    Token kw = advance(); // PRINT

    Stmt s;
    s.type = StmtType::Print;
    s.loc = kw.loc;
    s.print_stmt = std::make_unique<PrintStmt>();

    while (true) {
        // 1. Separator?
        if (match(TokenType::Semicolon) || match(TokenType::Comma)) {
            Token sep = advance();
            PrintElem e;
            e.type = PrintElemType::Separator;
            e.sep = sep.type;
            s.print_stmt->elems.push_back(std::move(e));
            continue;
        }

        // 2. String literal?
        if (match(TokenType::StringLiteral)) {
            Token t = advance();
            PrintElem e;
            e.type = PrintElemType::String;
            e.text = t.text;
            s.print_stmt->elems.push_back(std::move(e));
            continue;
        }

        // 3. Expression?
        if (starts_expression(peek())) {
            Expr ex = parse_expr();
            PrintElem e;
            e.type = PrintElemType::Expr;
            e.expr = std::move(ex);
            s.print_stmt->elems.push_back(std::move(e));
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
    s.loc = kw.loc;
    s.if_stmt = std::make_unique<IfStmt>();

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
    auto if_stmt = std::make_unique<IfStmt>();
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

Stmt Parser::parse_while() {
    Token kw = advance(); // WHILE

    auto w = std::make_unique<WhileStmt>();
    w->condition = parse_expr();

    // Require newline after condition
    expect(TokenType::Newline, "Expected newline after WHILE condition");

    // Body until WEND
    w->body = parse_block_until({ TokenType::KeywordWEnd }, "WEND");

    // Consume WEND and its newline
    expect(TokenType::KeywordWEnd, "Expected WEND");
    if (!match(TokenType::Newline)) {
        error_here("Expected newline after WEND");
    }

    Stmt s;
    s.type = StmtType::While;
    s.loc = kw.loc;
    s.while_stmt = std::move(w);
    return s;
}

Stmt Parser::parse_for() {
    Token kw = advance(); // FOR

    // Parse loop variable
    Token for_var = expect(TokenType::Identifier, "Expected loop variable after FOR");
    expect(TokenType::Equal, "Expected '=' after loop variable");

    auto f = std::make_unique<ForStmt>();
    f->var = for_var.text;

    // start-expr
    f->start_expr = parse_expr();

    expect(TokenType::KeywordTo, "Expected TO");

    // end-expr
    f->end_expr = parse_expr();

    // Optional STEP
    if (match(TokenType::KeywordStep)) {
        advance();
        f->step_expr = parse_expr();
    }
    else {
        // default step = 1
        f->step_expr = Expr::number(1, kw.loc);
    }

    // newline required
    if (!match(TokenType::Newline)) {
        error_here("Expected newline after FOR header");
    }

    // Body until NEXT
    f->body = parse_block_until({ TokenType::KeywordNext }, "NEXT");

    // Consume NEXT + newline
    expect(TokenType::KeywordNext, "Expected NEXT");

    // Optional variable
    Token next_var = peek();
    if (next_var.type == TokenType::Identifier) {
        if (for_var.text != next_var.text) {
            error_here("NEXT variable '" + next_var.text +
                       "' does not match FOR variable '" + for_var.text + "'");
        }
        advance();
    }

    // end of line
    if (!match(TokenType::Newline)) {
        error_here("Expected newline after NEXT");
    }

    Stmt s;
    s.type = StmtType::For;
    s.loc = kw.loc;
    s.for_stmt = std::move(f);
    return s;
}

Stmt Parser::parse_dim() {
    advance(); // DIM

    Token var = expect(TokenType::Identifier, "Expected array name after DIM");
    expect(TokenType::LParen, "Expected '(' after array name");
    Expr size_expr = parse_expr();
    expect(TokenType::RParen, "Expected ')' after size expression");

    // newline
    if (!match(TokenType::Newline)) {
        error_here("Expected newline after DIM");
    }

    Stmt s;
    s.type = StmtType::Dim;
    s.dim_stmt = std::make_unique<DimStmt>();
    s.dim_stmt->var = var.text;
    s.dim_stmt->size_expr = std::make_unique<Expr>(std::move(size_expr));
    return s;
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
    case TokenType::StringLiteral:
    case TokenType::Identifier:
    case TokenType::LParen:
    case TokenType::KeywordLeftDollar:
    case TokenType::KeywordMidDollar:
    case TokenType::KeywordRightDollar:
    case TokenType::KeywordStrDollar:
    case TokenType::KeywordLen:
    case TokenType::KeywordVal:
    case TokenType::KeywordChrDollar:
    case TokenType::KeywordAsc:
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
        left = Expr::binop(op.type, std::move(left), std::move(right), op.loc);
    }

    return left;
}

Expr Parser::parse_xor() {
    Expr left = parse_and();

    while (match(TokenType::KeywordXor)) {
        Token op = advance();
        Expr right = parse_and();
        left = Expr::binop(op.type, std::move(left), std::move(right), op.loc);
    }

    return left;
}

Expr Parser::parse_and() {
    Expr left = parse_relational();

    while (match(TokenType::KeywordAnd)) {
        Token op = advance();
        Expr right = parse_relational();
        left = Expr::binop(op.type, std::move(left), std::move(right), op.loc);
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
        left = Expr::binop(op.type, std::move(left), std::move(right), op.loc);
    }

    return left;
}

Expr Parser::parse_shift() {
    Expr left = parse_add();

    while (match(TokenType::KeywordShl) || match(TokenType::KeywordShr)) {
        Token op = advance();
        Expr right = parse_add();
        left = Expr::binop(op.type, std::move(left), std::move(right), op.loc);
    }

    return left;
}

Expr Parser::parse_add() {
    Expr left = parse_mul();

    while (match(TokenType::Plus) ||
            match(TokenType::Minus) ||
            match(TokenType::Ampersand)) {
        Token op = advance();
        Expr right = parse_mul();

        if (op.type == TokenType::Ampersand) {
            left = Expr::concat(std::move(left), std::move(right), op.loc);
        }
        else {
            left = Expr::binop(op.type, std::move(left), std::move(right), op.loc);
        }
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
        left = Expr::binop(op.type, std::move(left), std::move(right), op.loc);
    }

    return left;
}

Expr Parser::parse_unary() {
    if (match(TokenType::Plus) || match(TokenType::Minus)) {
        Token op = advance();
        Expr inner = parse_unary();
        return Expr::unary(op.type, std::move(inner), op.loc);
    }
    if (match(TokenType::KeywordNot)) {
        Token op = advance();
        Expr inner = parse_unary();
        return Expr::unary(op.type, std::move(inner), op.loc);
    }
    return parse_power();
}

Expr Parser::parse_power() {
    Expr left = parse_primary();
    if (match(TokenType::Caret)) {
        Token op = advance();
        // right-associative; no unary sign allowed directly on the exponent
        Expr right = parse_power();
        left = Expr::binop(op.type, std::move(left), std::move(right), op.loc);
    }
    return left;
}

Expr Parser::parse_primary() {
    Token t = peek();

    if (match(TokenType::Number)) {
        advance();
        return Expr::number(t.value, t.loc);
    }

    if (match(TokenType::StringLiteral)) {
        advance();
        return Expr::string_literal(t.text, t.loc);
    }

    if (match_any({
    TokenType::KeywordLeftDollar,
    TokenType::KeywordMidDollar,
    TokenType::KeywordRightDollar,
    TokenType::KeywordStrDollar,
    TokenType::KeywordLen,
    TokenType::KeywordVal,
    TokenType::KeywordChrDollar,
    TokenType::KeywordAsc })) {

        Token fn = advance();
        expect(TokenType::LParen, "Expected '(' after function name");

        std::vector<std::unique_ptr<Expr>> args;
        if (!match(TokenType::RParen)) {
            args.push_back(std::make_unique<Expr>(parse_expr()));
            while (match(TokenType::Comma)) {
                advance();
                args.push_back(std::make_unique<Expr>(parse_expr()));
            }
        }

        expect(TokenType::RParen, "Expected ')' after function arguments");
        return Expr::call(fn, std::move(args), fn.loc);
    }

    if (match(TokenType::Identifier)) {
        Token var = advance();
        bool is_string = is_string_var(var.text);

        if (match(TokenType::LParen)) {
            if (is_string) {
                error_at(var.loc,
                         "string variable '" + var.text + "' cannot be indexed");
            }
            advance();
            auto index = std::make_unique<Expr>(parse_expr());
            expect(TokenType::RParen, "Expected ')'");
            return Expr::array_access(var.text, std::move(index), t.loc);
        }

        return Expr::var(var.text, t.loc);
    }

    if (match(TokenType::LParen)) {
        advance();
        Expr e = parse_expr();
        expect(TokenType::RParen, "Expected ')'");
        return e;
    }

    error_here("Expected number, string, variable, function call, unary operator, or '('");
}
