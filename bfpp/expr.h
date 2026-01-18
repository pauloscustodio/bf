//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "parser.h"

class ExpressionParser {
public:
    explicit ExpressionParser(Parser& p);

    int parse_expression();

private:
    Parser& p_;

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
};
