//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "lexer.h"
#include "utils.h"
#include <iostream>

Lexer::Lexer(const std::string& src)
    : src(src), pos(0), line(1) {
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skip_whitespace();

        if (eof()) {
            tokens.push_back(make(TokenType::EndOfFile, "", 0));
            break;
        }

        char c = peek();
        char n = peek_next();

        // newline as statement separator
        if (c == '\n') {
            advance();
            tokens.push_back(make(TokenType::Newline, "\\n", 0));
            continue;
        }

        // Line continuation: underscore followed by newline
        if (c == '_') {
            if (n == '\n') {
                advance(); // skip '_'
                advance(); // skip '\n'
                continue;
            }
            else if (n == '\0') {
                error_here("Line continuation '_' must be followed by a newline");
            }
            else {
                error_here("Unexpected '_' in expression or statement");
            }
        }

        // string literals
        if (c == '"') {
            tokens.push_back(string_literal());
            continue;
        }

        // identifiers or keywords
        if (is_alpha(c)) {
            tokens.push_back(identifier_or_keyword());
            continue;
        }

        // numbers
        if (is_digit(c)) {
            tokens.push_back(number());
            continue;
        }

        // Two-character operators first
        if (c == '<' && n == '=') {
            advance();
            advance();
            tokens.push_back(make(TokenType::LessEqual, "<=", 0));
            continue;
        }

        if (c == '>' && n == '=') {
            advance();
            advance();
            tokens.push_back(make(TokenType::GreaterEqual, ">=", 0));
            continue;
        }

        if (c == '<' && n == '>') {
            advance();
            advance();
            tokens.push_back(make(TokenType::NotEqual, "<>", 0));
            continue;
        }

        if (c == '<' && n == '<') {
            advance();
            advance();
            tokens.push_back(make(TokenType::KeywordShl, "<<", 0));
            continue;
        }

        if (c == '>' && n == '>') {
            advance();
            advance();
            tokens.push_back(make(TokenType::KeywordShr, ">>", 0));
            continue;
        }

        // single character operators
        switch (c) {
        case '+':
            tokens.push_back(simple(TokenType::Plus));
            continue;
        case '-':
            tokens.push_back(simple(TokenType::Minus));
            continue;
        case '*':
            tokens.push_back(simple(TokenType::Star));
            continue;
        case '/':
        case'\\':
            tokens.push_back(simple(TokenType::Slash));
            continue;
        case '^':
            tokens.push_back(simple(TokenType::Caret));
            continue;
        case '=':
            tokens.push_back(simple(TokenType::Equal));
            continue;
        case '<':
            tokens.push_back(simple(TokenType::Less));
            continue;
        case '>':
            tokens.push_back(simple(TokenType::Greater));
            continue;
        case '(':
            tokens.push_back(simple(TokenType::LParen));
            continue;
        case ')':
            tokens.push_back(simple(TokenType::RParen));
            continue;
        case ':':
            tokens.push_back(simple(TokenType::Colon));
            continue;
        case ';':
            tokens.push_back(simple(TokenType::Semicolon));
            continue;
        case ',':
            tokens.push_back(simple(TokenType::Comma));
            continue;
        }

        error_here("Unexpected character '" + std::string(1, c) + "'");
    }

    return tokens;
}

bool Lexer::eof() const {
    return pos >= src.size();
}

char Lexer::peek() const {
    if (pos >= src.size()) {
        return '\0';
    }
    return src[pos];
}

char Lexer::peek_next() const {
    if (pos + 1 >= src.size()) {
        return '\0';
    }
    return src[pos + 1];
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') {
        line++;
    }
    return c;
}

[[noreturn]]
void Lexer::error_here(const std::string& msg) const {
    std::cerr << "Error at line " << line << ": " << msg << std::endl;
    exit(EXIT_FAILURE);
}

Token Lexer::make(TokenType type, const std::string& text, int value) const {
    Token t;
    t.type = type;
    t.text = text;
    t.value = value;
    t.line = line;
    return t;
}

Token Lexer::simple(TokenType type) {
    char c = advance();
    std::string s(1, c);
    return make(type, s, 0);
}

void Lexer::skip_whitespace() {
    while (!eof()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f') {
            advance();
            continue;
        }

        // BASIC comment: skip until end of line
        if (c == '\'') {
            while (!eof() && peek() != '\n') {
                advance();
            }
            continue;
        }
        break;
    }
}

Token Lexer::identifier_or_keyword() {
    size_t start = pos;

    while (!eof() && (is_alnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string text = src.substr(start, pos - start);

    // uppercase for keyword matching AND variable normalization
    std::string upper = uppercase(text);

    if (upper == "LET")
        return Token{ TokenType::KeywordLet, text, 0, line };
    if (upper == "INPUT")
        return Token{ TokenType::KeywordInput, text, 0, line };
    if (upper == "PRINT")
        return Token{ TokenType::KeywordPrint, text, 0, line };
    if (upper == "IF")
        return Token{ TokenType::KeywordIf, text, 0, line };
    if (upper == "THEN")
        return Token{ TokenType::KeywordThen, text, 0, line };
    if (upper == "ELSE")
        return Token{ TokenType::KeywordElse, text, 0, line };
    if (upper == "ENDIF")
        return Token{ TokenType::KeywordEndIf, text, 0, line };
    if (upper == "WHILE")
        return Token{ TokenType::KeywordWhile, text, 0, line };
    if (upper == "WEND")
        return Token{ TokenType::KeywordWEnd, text, 0, line };
    if (upper == "MOD")
        return Token{ TokenType::KeywordMod, text, 0, line };
    if (upper == "SHL")
        return Token{ TokenType::KeywordShl, text, 0, line };
    if (upper == "SHR")
        return Token{ TokenType::KeywordShr, text, 0, line };
    if (upper == "NOT")
        return Token{ TokenType::KeywordNot, text, 0, line };
    if (upper == "AND")
        return Token{ TokenType::KeywordAnd, text, 0, line };
    if (upper == "OR")
        return Token{ TokenType::KeywordOr, text, 0, line };
    if (upper == "XOR")
        return Token{ TokenType::KeywordXor, text, 0, line };

    // IDENTIFIER - store uppercase name in text
    return Token{ TokenType::Identifier, upper, 0, line };
}

Token Lexer::number() {
    size_t start = pos;

    while (!eof() && is_digit(peek())) {
        advance();
    }

    std::string text = src.substr(start, pos - start);
    int value = std::stoi(text);

    return Token{ TokenType::Number, text, value, line };
}

Token Lexer::string_literal() {
    advance(); // skip opening quote

    std::string value;

    while (!eof()) {
        char c = advance();

        if (c == '"') {
            // Check for doubled quote
            if (peek() == '"') {
                advance(); // consume second quote
                value.push_back('"');
                continue;
            }
            // End of string
            return make(TokenType::StringLiteral, value, 0);
        }

        value.push_back(c);
    }

    error_here("Unterminated string literal");
}
