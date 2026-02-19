//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

enum class TokenType {
    Identifier,
    Number,
    Let,
    Input,
    Print,
    Plus, Minus, Star, Slash, Caret,
    Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
    LParen, RParen,
    Mod, Shl, Shr,
    Not, And, Or, Xor,
    Newline,
    EndOfFile
};

struct Token {
    TokenType type = TokenType::EndOfFile;
    std::string text;
    int value = 0;          // only for numbers
    int line = 0;
    int column = 0;
};

class Lexer {
public:
    Lexer(const std::string& src);
    std::vector<Token> tokenize();

private:
    const std::string& src;
    size_t pos = 0;
    int line = 0, column = 0;

    bool eof() const;
    char peek() const;
    char peek_next() const;
    char advance();
    [[noreturn]] void error_here(const std::string& msg) const;
    Token make(TokenType type, const std::string& text, int value) const;
    Token simple(TokenType type);
    void skip_whitespace();
    Token identifier_or_keyword();
    Token number();
};
