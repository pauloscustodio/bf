//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "expr.h"
#include "parser.h"

ExpressionParser::ExpressionParser(Parser& parser)
    : p_(parser) {
}

int ExpressionParser::parse_expression() {
    int value = parse_logical_or();

    // Expect EndOfExpression
    if (p_.current().type != TokenType::EndOfExpression) {
        g_error_reporter.report_error(
            p_.current().loc,
            "unexpected token at end of #if expression"
        );
    }

    // Consume EndOfExpression
    if (p_.current().type == TokenType::EndOfExpression)
        p_.advance();

    return value;
}

int ExpressionParser::parse_logical_and() {
    int left = parse_bitwise_or();

    while (p_.current().type == TokenType::Operator &&
        p_.current().text == "&&") {
        p_.advance();
        int right = parse_bitwise_or();
        left = (left && right) ? 1 : 0;
    }

    return left;
}

int ExpressionParser::parse_logical_or() {
    int left = parse_logical_and();

    while (p_.current().type == TokenType::Operator &&
        p_.current().text == "||") {
        p_.advance();
        int right = parse_logical_and();
        left = (left || right) ? 1 : 0;
    }

    return left;
}

int ExpressionParser::parse_bitwise_and() {
    int left = parse_equality();

    while (p_.current().type == TokenType::Operator &&
        p_.current().text == "&") {
        p_.advance();
        int right = parse_equality();
        left = left & right;
    }

    return left;
}

int ExpressionParser::parse_bitwise_xor() {
    int left = parse_bitwise_and();

    while (p_.current().type == TokenType::Operator &&
        p_.current().text == "^") {
        p_.advance();
        int right = parse_bitwise_and();
        left = left ^ right;
    }

    return left;
}

int ExpressionParser::parse_bitwise_or() {
    int left = parse_bitwise_xor();

    while (p_.current().type == TokenType::Operator &&
        p_.current().text == "|") {
        p_.advance();
        int right = parse_bitwise_xor();
        left = left | right;
    }

    return left;
}

int ExpressionParser::parse_equality() {
    int left = parse_relational();

    while (p_.current().type == TokenType::Operator) {
        std::string op = p_.current().text;
        if (op != "==" && op != "!=")
            break;

        p_.advance();
        int right = parse_relational();

        if (op == "==") left = (left == right);
        else left = (left != right);
    }

    return left;
}

int ExpressionParser::parse_relational() {
    int left = parse_shift();

    while (p_.current().type == TokenType::Operator) {
        std::string op = p_.current().text;
        if (op != "<" && op != "<=" &&
            op != ">" && op != ">=")
            break;

        p_.advance();
        int right = parse_shift();

        if (op == "<") left = (left < right);
        else if (op == "<=") left = (left <= right);
        else if (op == ">") left = (left > right);
        else left = (left >= right);
    }

    return left;
}

int ExpressionParser::parse_shift() {
    int left = parse_additive();

    while (p_.current().type == TokenType::Operator) {
        std::string op = p_.current().text;
        if (op != "<<" && op != ">>")
            break;

        p_.advance();
        int right = parse_additive();

        if (right < 0) {
            g_error_reporter.report_error(
                p_.current().loc,
                "negative shift count"
            );
            continue;
        }

        if (op == "<<") left = left << right;
        else left = left >> right;
    }

    return left;
}

int ExpressionParser::parse_additive() {
    int left = parse_multiplicative();

    while (p_.current().type == TokenType::Operator) {
        std::string op = p_.current().text;
        if (op != "+" && op != "-")
            break;

        p_.advance();
        int right = parse_multiplicative();

        if (op == "+") left += right;
        else left -= right;
    }

    return left;
}

int ExpressionParser::parse_multiplicative() {
    int left = parse_unary();

    while (p_.current().type == TokenType::Operator) {
        std::string op = p_.current().text;
        if (op != "*" && op != "/" && op != "%")
            break;

        p_.advance();
        int right = parse_unary();

        if (op == "*") left = left * right;
        else if (op == "/") {
            if (right == 0) {
                g_error_reporter.report_error(
                    p_.current().loc,
                    "division by zero"
                );
                left = 0;
            }
            else left = left / right;
        }
        else if (op == "%") {
            if (right == 0) {
                g_error_reporter.report_error(
                    p_.current().loc,
                    "modulo by zero"
                );
                left = 0;
            }
            else left = left % right;
        }
    }

    return left;
}

int ExpressionParser::parse_unary() {
    const Token& tok = p_.current();

    if (tok.type == TokenType::Operator) {
        std::string op = tok.text;

        // defined operator
        if (op == "defined") {
            p_.advance();

            bool paren = false;
            if (p_.current().type == TokenType::LParen) {
                paren = true;
                p_.advance();
            }

            if (p_.current().type != TokenType::Identifier) {
                g_error_reporter.report_error(
                    p_.current().loc,
                    "expected identifier after defined"
                );
                return 0;
            }

            bool is_def = (g_macro_table.lookup(p_.current().text) != nullptr);
            p_.advance();

            if (paren) {
                if (p_.current().type != TokenType::RParen) {
                    g_error_reporter.report_error(
                        p_.current().loc,
                        "expected ')'"
                    );
                }
                else {
                    p_.advance();
                }
            }

            return is_def ? 1 : 0;
        }

        // unary operators
        if (op == "!" || op == "+" || op == "-" || op == "~") {
            p_.advance();
            int v = parse_unary();
            if (op == "!") return !v;
            if (op == "+") return +v;
            if (op == "-") return -v;
            if (op == "~") return ~v;
        }
    }

    return parse_primary();
}

int ExpressionParser::parse_primary() {
    const Token& tok = p_.current();

    if (tok.type == TokenType::Integer) {
        int v = tok.int_value;
        p_.advance();
        return v;
    }

    if (tok.type == TokenType::Identifier) {
        int v = value_of_identifier(tok);
        p_.advance();
        return v;
    }

    if (tok.type == TokenType::LParen) {
        p_.advance();
        int v = parse_logical_or();
        if (p_.current().type != TokenType::RParen) {
            g_error_reporter.report_error(
                p_.current().loc,
                "expected ')'"
            );
        }
        else {
            p_.advance();
        }
        return v;
    }

    g_error_reporter.report_error(tok.loc, "unexpected token in #if expression");
    p_.advance();
    return 0;
}

int ExpressionParser::value_of_identifier(const Token& tok) {
    const Macro* macro = g_macro_table.lookup(tok.text);
    if (!macro)
        return 0;

    // Only object-like macros with a single integer token
    if (!macro->params.empty())
        return 0;

    if (macro->body.size() != 1)
        return 0;

    const Token& body = macro->body[0];
    if (body.type != TokenType::Integer)
        return 0;

    return body.int_value;
}
