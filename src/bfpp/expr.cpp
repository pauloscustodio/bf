//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "expr.h"
#include "parser.h"
#include <cassert>
#include <unordered_map>

const std::unordered_map<std::string, ExpressionParser::FunctionHandler>
ExpressionParser::kFunctions = {
    { "global",     &ExpressionParser::handle_global     },
    { "temp",       &ExpressionParser::handle_temp       },
    { "arg",        &ExpressionParser::handle_arg        },
    { "local",      &ExpressionParser::handle_local      },
    { "local_temp", &ExpressionParser::handle_local_temp },
};

ExpressionParser::ExpressionParser(TokenSource& source, Parser* parser,
                                   bool undefined_as_zero)
    : source_(source), parser_(parser)
    , output_(parser ? & parser->output() : nullptr)
    , undefined_as_zero_(undefined_as_zero) {
}

bool ExpressionParser::is_function_name(const std::string& name) {
    return kFunctions.find(name) != kFunctions.end();
}

int ExpressionParser::parse_expression() {
    return parse_logical_or();
}

int ExpressionParser::parse_logical_and() {
    int left = parse_bitwise_or();

    while (source_.current().type == TokenType::Operator &&
            source_.current().text == "&&") {
        source_.advance();
        int right = parse_bitwise_or();
        left = (left && right) ? 1 : 0;
    }

    return left;
}

int ExpressionParser::parse_logical_or() {
    int left = parse_logical_and();

    while (source_.current().type == TokenType::Operator &&
            source_.current().text == "||") {
        source_.advance();
        int right = parse_logical_and();
        left = (left || right) ? 1 : 0;
    }

    return left;
}

int ExpressionParser::parse_bitwise_and() {
    int left = parse_equality();

    while (source_.current().type == TokenType::Operator &&
            source_.current().text == "&") {
        source_.advance();
        int right = parse_equality();
        left = left & right;
    }

    return left;
}

int ExpressionParser::parse_bitwise_xor() {
    int left = parse_bitwise_and();

    while (source_.current().type == TokenType::Operator &&
            source_.current().text == "^") {
        source_.advance();
        int right = parse_bitwise_and();
        left = left ^ right;
    }

    return left;
}

int ExpressionParser::parse_bitwise_or() {
    int left = parse_bitwise_xor();

    while (source_.current().type == TokenType::Operator &&
            source_.current().text == "|") {
        source_.advance();
        int right = parse_bitwise_xor();
        left = left | right;
    }

    return left;
}

int ExpressionParser::parse_equality() {
    int left = parse_relational();

    while (source_.current().type == TokenType::Operator) {
        std::string op = source_.current().text;
        if (op != "==" && op != "!=") {
            break;
        }

        source_.advance();
        int right = parse_relational();

        if (op == "==") {
            left = (left == right);
        }
        else {
            left = (left != right);
        }
    }

    return left;
}

int ExpressionParser::parse_relational() {
    int left = parse_shift();

    while (source_.current().type == TokenType::Operator) {
        std::string op = source_.current().text;
        if (op != "<" && op != "<=" &&
                op != ">" && op != ">=") {
            break;
        }

        source_.advance();
        int right = parse_shift();

        if (op == "<") {
            left = (left < right);
        }
        else if (op == "<=") {
            left = (left <= right);
        }
        else if (op == ">") {
            left = (left > right);
        }
        else {
            left = (left >= right);
        }
    }

    return left;
}

int ExpressionParser::parse_shift() {
    int left = parse_additive();

    while (source_.current().type == TokenType::Operator) {
        std::string op = source_.current().text;
        if (op != "<<" && op != ">>") {
            break;
        }

        source_.advance();
        int right = parse_additive();

        if (right < 0) {
            g_error_reporter.report_error(
                source_.current().loc,
                "negative shift count"
            );
            continue;
        }

        if (op == "<<") {
            left = left << right;
        }
        else {
            left = left >> right;
        }
    }

    return left;
}

int ExpressionParser::parse_additive() {
    int left = parse_multiplicative();

    while (source_.current().type == TokenType::Operator) {
        std::string op = source_.current().text;
        if (op != "+" && op != "-") {
            break;
        }

        source_.advance();
        int right = parse_multiplicative();

        if (op == "+") {
            left += right;
        }
        else {
            left -= right;
        }
    }

    return left;
}

int ExpressionParser::parse_multiplicative() {
    int left = parse_unary();

    while (source_.current().type == TokenType::Operator) {
        std::string op = source_.current().text;
        if (op != "*" && op != "/" && op != "%") {
            break;
        }

        source_.advance();
        int right = parse_unary();

        if (op == "*") {
            left = left * right;
        }
        else if (op == "/") {
            if (right == 0) {
                g_error_reporter.report_error(
                    source_.current().loc,
                    "division by zero"
                );
                left = 0;
            }
            else {
                left = left / right;
            }
        }
        else if (op == "%") {
            if (right == 0) {
                g_error_reporter.report_error(
                    source_.current().loc,
                    "modulo by zero"
                );
                left = 0;
            }
            else {
                left = left % right;
            }
        }
    }

    return left;
}

int ExpressionParser::parse_unary() {
    const Token& tok = source_.current();

    if (tok.type == TokenType::Operator) {
        std::string op = tok.text;

        // defined operator
        if (op == "defined") {
            source_.advance();

            bool paren = false;
            if (source_.current().type == TokenType::LParen) {
                paren = true;
                source_.advance();
            }

            if (source_.current().type != TokenType::Identifier) {
                g_error_reporter.report_error(
                    source_.current().loc,
                    "expected identifier after defined"
                );
                return 0;
            }

            bool is_def = (g_macro_table.lookup(source_.current().text) != nullptr);
            source_.advance();

            if (paren) {
                if (source_.current().type != TokenType::RParen) {
                    g_error_reporter.report_error(
                        source_.current().loc,
                        "expected ')'"
                    );
                }
                else {
                    source_.advance();
                }
            }

            return is_def ? 1 : 0;
        }

        // unary operators
        if (op == "!" || op == "+" || op == "-" || op == "~") {
            source_.advance();
            int v = parse_unary();
            if (op == "!") {
                return !v;
            }
            if (op == "+") {
                return +v;
            }
            if (op == "-") {
                return -v;
            }
            if (op == "~") {
                return ~v;
            }
        }
    }

    return parse_primary();
}

int ExpressionParser::parse_primary() {
    const Token& tok = source_.current();

    if (tok.type == TokenType::Integer) {
        int v = tok.int_value;
        source_.advance();
        return v;
    }

    if (tok.type == TokenType::Identifier) {
        // Function call with address helpers
        if (is_function_name(tok.text)) {
            auto it = kFunctions.find(tok.text);
            const Token func_tok = tok;
            source_.advance(); // consume function name

            if (source_.current().type != TokenType::LParen) {
                g_error_reporter.report_error(
                    source_.current().loc,
                    "expected '(' after function name '" + func_tok.text + "'"
                );
                return 0;
            }

            source_.advance(); // consume '('
            int arg = parse_expression();

            if (source_.current().type != TokenType::RParen) {
                g_error_reporter.report_error(
                    source_.current().loc,
                    "expected ')'"
                );
            }
            else {
                source_.advance(); // consume ')'
            }

            if (it != kFunctions.end()) {
                return (this->*it->second)(func_tok, arg);
            }
            return 0;
        }

        int v = value_of_identifier(tok);
        source_.advance();
        return v;
    }

    if (tok.type == TokenType::LParen) {
        source_.advance();
        int v = parse_expression();
        if (source_.current().type != TokenType::RParen) {
            g_error_reporter.report_error(
                source_.current().loc,
                "expected ')'"
            );
        }
        else {
            source_.advance();
        }
        return v;
    }

    g_error_reporter.report_error(tok.loc, "unexpected token in expression");
    source_.advance();
    return 0;
}

int ExpressionParser::value_of_identifier(const Token& tok) {
    std::unordered_set<std::string> expanding;
    return eval_macro_recursive(tok, expanding);
}

int ExpressionParser::eval_macro_recursive(const Token& tok,
        std::unordered_set<std::string>& expanding) {
    std::string name = tok.text;
    const Macro* macro = g_macro_table.lookup(name);
    if (!macro) {
        if (undefined_as_zero_) {
            return 0; // undefined treated as 0 without error
        }
        g_error_reporter.report_error(
            tok.loc,
            "macro '" + name + "' is not defined"
        );
        source_.advance();
        return 0;
    }
    if (!macro->params.empty()) {
        g_error_reporter.report_error(
            tok.loc,
            "macro '" + name + "' is not an object-like macro"
        );
        g_error_reporter.report_note(
            macro->loc,
            "macro '" + name + "' defined here"
        );
        source_.advance();
        return 0;
    }

    // Circular reference check
    if (expanding.count(name)) {
        g_error_reporter.report_error(
            macro->loc,
            "circular macro expansion in expression"
        );
        source_.advance();
        return 0;
    }

    expanding.insert(name);
    ArrayTokenSource source(macro->body);
    ExpressionParser expr(source, parser_, undefined_as_zero_);
    int result = expr.parse_expression();
    expanding.erase(name);

    return result;
}

int ExpressionParser::handle_global(const Token& tok, int argument) {
    assert(output_); // global() requires an active parser
    return output_->global_address(tok, argument);
}

int ExpressionParser::handle_temp(const Token& tok, int argument) {
    assert(output_); // temp() requires an active parser
    return output_->temp_address(tok, argument);
}

int ExpressionParser::handle_arg(const Token& tok, int argument) {
    assert(output_); // arg() requires an active parser
    return output_->frame_arg_address(tok, argument);
}

int ExpressionParser::handle_local(const Token& tok, int argument) {
    assert(output_); // local() requires an active parser
    return output_->frame_local_address(tok, argument);
}

int ExpressionParser::handle_local_temp(const Token& tok, int argument) {
    assert(output_); // local_temp() requires an active parser
    return output_->frame_temp_address(tok, argument);
}
