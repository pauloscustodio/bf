//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "lexer.h"

Token Lexer::get() {
    if (!pushback_.empty()) {
        Token t = pushback_.front();
        pushback_.pop_front();
        return t;
    }

    while (true) {
        // comments are skipped in both modes
        if (skip_comment())
            continue;

        // whitespace depends on mode
        if (mode_ == ExpansionContext::Normal)
            skip_whitespace_normal();
        else
            skip_whitespace_expr();

        // if no more whitespace/comments to skip, break
        if (!skip_comment())
            break;
    }

    // Now lex the next token based on mode
    if (mode_ == ExpansionContext::Normal)
        return next_token_normal();
    else
        return next_token_expr();
}

Token Lexer::peek() {
    Token t = get();
    pushback_.push_front(t);
    return t;
}

void Lexer::unget(const Token& t) {
    pushback_.push_front(t);
}

Token Lexer::next_token_normal() {
    while (true) {
        skip_whitespace();

        int ch = get_char();
        SourceLocation loc = g_file_stack.location();

        if (ch == EOF) {
            return make_token(TokenType::EndOfFile, "", loc);
        }

        // Directives: #identifier
        if (ch == '#') {
            return lex_identifier_or_directive(ch, loc);
        }

        // Identifiers: [A-Za-z_][A-Za-z0-9_]*
        if (is_alpha(ch) || ch == '_') {
            return lex_identifier_or_directive(ch, loc);
        }

        // Numbers: [0-9]+
        if (is_digit(ch)) {
            return lex_number(ch, loc);
        }

        // String literal
        if (ch == '"') {
            return lex_string(loc);
        }

        // Brainfuck characters
        if (ch == '+' || ch == '-' || ch == '<' || ch == '>' ||
            ch == '[' || ch == ']' || ch == '.' || ch == ',') {
            std::string s(1, static_cast<char>(ch));
            return make_token(TokenType::BFInstr, s, loc);
        }

        // Punctuation
        switch (ch) {
        case '(': return make_token(TokenType::LParen, "(", loc);
        case ')': return make_token(TokenType::RParen, ")", loc);
        case ',': return make_token(TokenType::Comma, ",", loc);
        }

        // Unknown character
        g_error_reporter.report_error(
            loc,
            "unexpected character '" + std::string(1, static_cast<char>(ch)) + "'"
        );
        return make_token(
            TokenType::Error,
            std::string(1, static_cast<char>(ch)),
            loc
        );
    }
}

Token Lexer::next_token_expr() {
    while (true) {
        skip_whitespace();

        char ch = peek_char();
        SourceLocation loc = g_file_stack.location();

        if (is_digit(ch)) return lex_number(ch, loc);
        if (is_alpha(ch) || ch == '_') return lex_identifier_or_directive(ch, loc);

        if (ch == '(') { get_char(); return Token(TokenType::LParen, "(", loc); }
        if (ch == ')') { get_char(); return Token(TokenType::RParen, ")", loc); }

        // Operators
        if (is_operator_char(ch)) {
            return lex_operator_token();
        }

        // End of expression: newline or end-of-file
        if (ch == '\n' || ch == EOF) {
            return Token(TokenType::EndOfExpression, "", loc);
        }

        // Anything else is invalid inside #if
        g_error_reporter.report_error(
            loc,
            "unexpected character in #if expression '" + std::string(1, static_cast<char>(ch)) + "'"
        );
        return make_token(
            TokenType::Error,
            std::string(1, static_cast<char>(ch)),
            loc
        );
    }
}

void Lexer::set_mode(ExpansionContext mode) { 
    mode_ = mode; 
}

int Lexer::get_char() {
    return g_file_stack.get_char();
}

int Lexer::peek_char() {
    int ch = get_char();
    if (ch != EOF) {
        unget_char();
    }
    return ch;
}

int Lexer::peek_next_char() {
    int ch1 = get_char();
    int ch2 = get_char();
    if (ch2 != EOF) {
        unget_char();
    }
    if (ch1 != EOF) {
        unget_char();
    }
    return ch2;
}

void Lexer::unget_char() {
    g_file_stack.unget_char();
}

bool Lexer::is_eof() const {
    return g_file_stack.is_eof();
}

Token Lexer::make_token(TokenType type, const std::string& text, const SourceLocation& loc) {
    return Token(type, text, loc);
}

Token Lexer::lex_identifier_or_directive(int first_ch, const SourceLocation& loc) {
    std::string text;
    text.push_back(static_cast<char>(first_ch));

    while (true) {
        int ch = get_char();
        if (is_alnum(ch) || ch == '_') {
            text.push_back(static_cast<char>(ch));
        }
        else {
            unget_char();
            break;
        }
    }

    // If it starts with '#', it's a directive
    if (first_ch == '#') {
        return make_token(TokenType::Directive, text, loc);
    }

    return make_token(TokenType::Identifier, text, loc);
}

Token Lexer::lex_number(int first_ch, const SourceLocation& loc) {
    std::string text;
    text.push_back(static_cast<char>(first_ch));

    while (true) {
        int ch = get_char();
        if (is_digit(ch)) {
            text.push_back(static_cast<char>(ch));
        }
        else {
            unget_char();
            break;
        }
    }

    Token tok = make_token(TokenType::Integer, text, loc);
    tok.int_value = std::stoi(text);
    return tok;
}

Token Lexer::lex_string(const SourceLocation& loc) {
    std::string text;

    while (true) {
        int ch = get_char();
        if (ch == EOF) {
            g_error_reporter.report_error(loc, "unterminated string literal");
            return make_token(TokenType::Error, text, loc);
        }
        if (ch == '"') {
            break;
        }
        text.push_back(static_cast<char>(ch));
    }

    return make_token(TokenType::String, text, loc);
}

bool Lexer::is_operator_char(char ch) {
    switch (ch) {
    case '+': case '-': case '*': case '/': case '%':
    case '&': case '|': case '^': case '~':
    case '!': case '<': case '>': case '=':
        return true;
    default:
        return false;
    }
}

Token Lexer::lex_operator_token() {
    char c1 = get_char();
    char c2 = peek_char();
    SourceLocation loc = g_file_stack.location();

    // Two-character operators
    if ((c1 == '&' && c2 == '&') ||
        (c1 == '|' && c2 == '|') ||
        (c1 == '=' && c2 == '=') ||
        (c1 == '!' && c2 == '=') ||
        (c1 == '<' && c2 == '=') ||
        (c1 == '>' && c2 == '=') ||
        (c1 == '<' && c2 == '<') ||
        (c1 == '>' && c2 == '>')) {

        get_char(); // consume second char
        return Token(TokenType::Operator, std::string() + c1 + c2, loc);
    }

    // Single-character operator
    return Token(TokenType::Operator, std::string(1, c1), loc);
}

void Lexer::skip_whitespace() {
    if (mode_ == ExpansionContext::Normal) {
        skip_whitespace_normal();
    }
    else {
        skip_whitespace_expr();
    }
}

void Lexer::skip_whitespace_normal() {
    while (true) {
        char ch = peek_char();
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f') {
            get_char();
            continue;
        }
        break;
    }
}

void Lexer::skip_whitespace_expr() {
    while (true) {
        char ch = peek_char();
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\v' || ch == '\f') {
            get_char();
            continue;
        }
        // DO NOT skip '\n'
        break;
    }
}

bool Lexer::skip_comment() {
    // Example: // comment until newline
    if (peek_char() == '/' && peek_next_char() == '/') {
        get_char(); get_char(); // consume //
        while (!is_eof() && peek_char() != '\n')
            get_char();
        return true;
    }

    // Example: /* block comment */
    if (peek_char() == '/' && peek_next_char() == '*') {
        get_char(); get_char(); // consume /*
        while (!is_eof()) {
            if (peek_char() == '*' && peek_next_char() == '/') {
                get_char(); get_char(); // consume */
                break;
            }
            get_char();
        }
        return true;
    }

    return false;
}

void Lexer::skip_multiline_comment(const SourceLocation& start_loc) {
    while (true) {
        int ch = get_char();

        if (ch == EOF) {
            g_error_reporter.report_error(
                start_loc,
                "unterminated multi-line comment"
            );
            return;
        }

        // Look for closing */
        if (ch == '*') {
            int next = get_char();
            if (next == '/') {
                return;
            }
            unget_char(); // not '/', continue scanning
        }
    }
}

