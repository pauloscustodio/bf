//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "files.h"
#include <string>

struct Line {
    std::string text;
    int line_num;
};

class CommentStripper {
public:
    bool getline(Line& out);
    bool is_eof() const;

private:
    bool in_block_comment_ = false;
};

enum class TokenType {
    EndOfInput,
    EndOfLine,
    Directive,      // #define, #include, #if, #else, #endif
    Identifier,
    Integer,
    String,
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    BFInstr,        // + - < > [ ] . ,
    Operator,       // "+", "-", "*", "/", "<<", "&&", etc.
};

struct Token {
    TokenType type = TokenType::EndOfInput;
    std::string text;   // original spelling
    int int_value = 0;  // only valid for Integer tokens
    SourceLocation loc;

    Token() = default;
    Token(TokenType t, const std::string& txt, const SourceLocation& loc)
        : type(t), text(txt), loc(loc) {
    }

    bool is_comma() const {
        return type == TokenType::BFInstr && text == ",";
    }

    static Token make_bf(char c, const SourceLocation& loc) {
        Token t;
        t.type = TokenType::BFInstr;
        t.text.assign(1, c);
        t.loc = loc;
        return t;
    }

    static Token make_int(int value, const SourceLocation& loc) {
        Token t;
        t.type = TokenType::Integer;
        t.int_value = value;
        t.text = std::to_string(value);
        t.loc = loc;
        return t;
    }
};

class Lexer {
public:
    Lexer(CommentStripper& stripper);
    Token get();
    Token peek(size_t offset = 0);
    bool at_end() const;

private:
    CommentStripper& stripper_;
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    bool in_directive_ = false;
    int expr_depth_ = 0;

    void scan_append(const Line& line);
    bool getline(Line& line);
};

bool is_identifier(const std::string& ident);
bool is_integer(const std::string& str);
