//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "files.h"
#include <deque>
#include <string>

inline bool is_alpha(int ch) {
    return std::isalpha(static_cast<unsigned char>(ch));
}

inline bool is_digit(int ch) {
    return std::isdigit(static_cast<unsigned char>(ch));
}

inline bool is_alnum(int ch) {
    return std::isalnum(static_cast<unsigned char>(ch));
}

enum class TokenType {
    Identifier,
    Integer,
    String,
    Directive,      // #define, #include, #if, #else, #endif
    BFInstr,        // + - < > [ ] . ,
    LParen,         // (
    RParen,         // )
    Comma,          // ,
    Operator,       // "+", "-", "*", "/", "<<", "&&", etc.
    EndOfExpression,// newline or EOF in #if expression mode
    EndOfFile,
    Error
};

struct Token {
    TokenType type = TokenType::Error;
    std::string text;   // original spelling
    int int_value = 0;  // only valid for Integer tokens
    SourceLocation loc;

    Token() = default;
    Token(TokenType t, const std::string& txt, const SourceLocation& location)
        : type(t), text(txt), loc(location) {
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

enum class ExpansionContext {
    Normal,
    DirectiveExpression,
    DirectiveKeyword
};

class Lexer {
public:
    Token get();
    Token peek();
    void unget(const Token& t);

    void set_mode(ExpansionContext mode);

private:
    std::deque<Token> pushback_;
    ExpansionContext mode_ = ExpansionContext::Normal;

    int get_char();
    int peek_char();
    int peek_next_char();
    void unget_char();
    bool is_eof() const;

    Token make_token(TokenType type, const std::string& text, const SourceLocation& loc);

    Token next_token_normal();
    Token next_token_expr();
    Token lex_number(int first_ch, const SourceLocation& loc);
    Token lex_identifier_or_directive(int first_ch, const SourceLocation& loc);
    Token lex_string(const SourceLocation& loc);
    bool is_operator_char(char ch);
    Token lex_operator_token();
    void skip_whitespace();
    void skip_whitespace_normal();
    void skip_whitespace_expr();
    bool skip_comment();
    void skip_multiline_comment(const SourceLocation& start_loc);
};

class TokenStream {
public:
    // Vector-backed constructor
    TokenStream(const std::vector<Token>& tokens)
        : tokens_ptr_(&tokens), pos_(0) {
    }

    // Single-token constructor
    TokenStream(const Token& single)
        : single_storage_{ single }, tokens_ptr_(&single_storage_), pos_(0) {
    }

    // Peek ahead by k tokens (default k = 0)
    const Token& peek(size_t k = 0) const {
        static Token eof(TokenType::EndOfFile, "", SourceLocation());

        size_t idx = pos_ + k;
        if (idx >= tokens_ptr_->size())
            return eof;

        return (*tokens_ptr_)[idx];
    }

    // Consume n tokens (default n = 1)
    void consume(size_t n = 1) {
        pos_ += n;
        if (pos_ > tokens_ptr_->size())
            pos_ = tokens_ptr_->size();
    }

    bool eof() const {
        return pos_ >= tokens_ptr_->size();
    }

private:
    std::vector<Token> single_storage_;      // used only in single-token mode
    const std::vector<Token>* tokens_ptr_;   // pointer to token storage
    size_t pos_ = 0;
};
