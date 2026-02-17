//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "lexer.h"
#include "utils.h"
#include <iostream>

Lexer::Lexer(const std::string& src)
    : src(src), pos(0), line(1), column(1) {
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

        // newline as statement separator
        if (c == '\n') {
            advance();
            tokens.push_back(make(TokenType::Newline, "\\n", 0));
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

        // operators
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
        case '=':
            tokens.push_back(simple(TokenType::Equal));
            continue;
        case '(':
            tokens.push_back(simple(TokenType::LParen));
            continue;
        case ')':
            tokens.push_back(simple(TokenType::RParen));
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
    return src[pos];
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') {
        line++;
        column = 1;
    }
    else {
        column++;
    }
    return c;
}

[[noreturn]]
void Lexer::error_here(const std::string& msg) const {
    std::cerr << "Error at line " << line << ", column " << column
              << ": " << msg << std::endl;
    exit(EXIT_FAILURE);
}

Token Lexer::make(TokenType type, const std::string& text, int value) const {
    return Token{ type, text, value, line, column };
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
    int start_col = column;
    size_t start = pos;

    while (!eof() && (is_alnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string text = src.substr(start, pos - start);

    // uppercase for keyword matching AND variable normalization
    std::string upper = uppercase(text);

    if (upper == "LET")
        return Token{ TokenType::KeywordLet, text, 0, line, start_col };
    if (upper == "INPUT")
        return Token{ TokenType::KeywordInput, text, 0, line, start_col };
    if (upper == "PRINT")
        return Token{ TokenType::KeywordPrint, text, 0, line, start_col };
    if (upper == "MOD")
        return Token{ TokenType::Mod, text, 0, line, start_col };
    if (upper == "SHL")
        return Token{ TokenType::Shl, text, 0, line, start_col };
    if (upper == "SHR")
        return Token{ TokenType::Shr, text, 0, line, start_col };

    // IDENTIFIER — store uppercase name in text
    return Token{ TokenType::Identifier, upper, 0, line, start_col };
}

Token Lexer::number() {
    int start_col = column;
    size_t start = pos;

    while (!eof() && is_digit(peek())) {
        advance();
    }

    std::string text = src.substr(start, pos - start);
    int value = std::stoi(text);

    return Token{ TokenType::Number, text, value, line, start_col };
}
