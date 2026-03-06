//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include "errors.h"

enum class TokenType {
#define X(name, str) name,
#include "token_type.def"
};

struct Token {
    TokenType type = TokenType::EndOfFile;
    std::string text;
    int value = 0;          // only for numbers
    SourceLoc loc;
};

class Lexer {
public:
    Lexer(const std::string& src);
    std::vector<Token> tokenize();

private:
    const std::string& src;
    size_t pos = 0;
    int line = 0;

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
    Token string_literal();
};

bool is_string_var(const std::string& name);
std::string token_type_to_string(TokenType type);
