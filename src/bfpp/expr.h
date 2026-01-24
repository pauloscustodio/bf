//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "parser.h"

class TokenSource {
public:
    virtual ~TokenSource() = default;
    virtual Token current() const = 0;
    virtual Token peek(size_t offset = 0) const = 0;
    virtual void advance() = 0;
    virtual bool at_end() const = 0;
};

class ParserTokenSource : public TokenSource {
public:
    ParserTokenSource(Parser& parser) : parser_(parser) {}
    Token current() const override { return parser_.current(); }
    Token peek(size_t offset = 0) const override { return parser_.peek(offset); }
    void advance() override { parser_.advance(); }
    bool at_end() const override { return parser_.current().type == TokenType::EndOfInput; }
private:
    Parser& parser_;
};

class ArrayTokenSource : public TokenSource {
public:
    ArrayTokenSource(const std::vector<Token>& tokens)
        : tokens_(tokens), pos_(0) {
    }
    Token current() const override {
        static Token eof(TokenType::EndOfInput, "", SourceLocation());
        return pos_ < tokens_.size() ? tokens_[pos_] : eof;
    }
    Token peek(size_t offset = 0) const override {
        static Token eof(TokenType::EndOfInput, "", SourceLocation());
        return (pos_ + offset) < tokens_.size() ? tokens_[pos_ + offset] : eof;
    }
    void advance() override { if (pos_ < tokens_.size()) ++pos_; }
    bool at_end() const override { return pos_ >= tokens_.size(); }
private:
    const std::vector<Token>& tokens_;
    size_t pos_;
};

class ExpressionParser {
public:
    ExpressionParser(TokenSource& source);

    int parse_expression();

private:
    TokenSource& source_;  // Single interface for both contexts

    int parse_logical_or();
    int parse_logical_and();
    int parse_bitwise_or();
    int parse_bitwise_xor();
    int parse_bitwise_and();
    int parse_equality();
    int parse_relational();
    int parse_shift();
    int parse_additive();
    int parse_multiplicative();
    int parse_unary();
    int parse_primary();

    int value_of_identifier(const Token& tok);
    int eval_macro_recursive(const Token& tok,
        std::unordered_set<std::string>& expanding);
};
