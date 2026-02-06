//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "expr.h"
#include "macros.h"
#include "parser.h"

MacroTable g_macro_table;

// Built-ins are now private to MacroExpander
const std::unordered_map<std::string, MacroExpander::BuiltinHandler> MacroExpander::kBuiltins = {
    { "alloc_cell", &MacroExpander::handle_alloc_cell },
    { "free_cell",  &MacroExpander::handle_free_cell  },
    { "clear",      &MacroExpander::handle_clear      },
    { "set",        &MacroExpander::handle_set        },
    { "move",       &MacroExpander::handle_move       },
    { "copy",       &MacroExpander::handle_copy       },
    { "not",        &MacroExpander::handle_not        },
    { "and",        &MacroExpander::handle_and        },
    { "or",         &MacroExpander::handle_or         },
    { "xor",        &MacroExpander::handle_xor        },
    { "if",         &MacroExpander::handle_if         },
    { "else",       &MacroExpander::handle_else       },
    { "endif",      &MacroExpander::handle_endif      },
    { "while",      &MacroExpander::handle_while      },
    { "endwhile",   &MacroExpander::handle_endwhile   },
    { "repeat",     &MacroExpander::handle_repeat     },
    { "endrepeat",  &MacroExpander::handle_endrepeat  },
};

bool MacroTable::define(const Macro& macro) {
    auto it = table_.find(macro.name);
    if (it != table_.end()) {
        // Error at the new definition
        g_error_reporter.report_error(
            macro.loc,
            "macro '" + macro.name + "' redefined"
        );

        // Note pointing to the original definition
        g_error_reporter.report_note(
            it->second.loc,
            "previous definition was here"
        );
        return false;
    }

    table_.emplace(macro.name, macro);
    return true;
}

void MacroTable::undef(const std::string& name) {
    table_.erase(name);
}

const Macro* MacroTable::lookup(const std::string& name) const {
    auto it = table_.find(name);
    if (it == table_.end()) {
        return nullptr;
    }
    return &it->second;
}

MacroExpander::MacroExpander(MacroTable& table)
    : table_(table) {
}

bool MacroExpander::try_expand(Parser& parser, const Token& token) {
    if (token.type != TokenType::Identifier) {
        return false;
    }

    if (auto it = kBuiltins.find(token.text); it != kBuiltins.end()) {
        // Builtins consume their own call; leave current_ on the next token.
        (this->*it->second)(parser, token);
        return true;
    }

    const Macro* macro = table_.lookup(token.text);
    if (!macro) {
        return false;
    }

    // Recursion guard
    if (expanding_.count(macro->name)) {
        g_error_reporter.report_error(token.loc,
                                      "macro '" + macro->name + "' expands to itself");
        return false;
    }

    // Collect arguments
    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, *macro, args)) {
        return false; // syntax error already reported
    }

    // Validate arity
    if (args.size() != macro->params.size()) {
        g_error_reporter.report_error(token.loc,
                                      "macro '" + macro->name + "' expects " +
                                      std::to_string(macro->params.size()) + " arguments");
        return false;
    }

    // Substitute and push to expansion stack
    expanding_.insert(macro->name);

    std::vector<Token> expanded = substitute_body(*macro, args);
    parser.push_macro_expansion(macro->name, expanded);
    // expanding_.erase() called in Parser::advance() when popping frame
    return true;
}

void MacroExpander::remove_expanding(const std::string& name) {
    expanding_.erase(name);
}

bool MacroExpander::collect_args(Parser& parser,
                                 const Macro& macro,
                                 std::vector<std::vector<Token>>& args) {
    args.clear();

    auto consume_to_eol = [&]() {
        while (parser.current_.type != TokenType::EndOfLine &&
                parser.current_.type != TokenType::EndOfInput) {
            parser.advance();
        }
    };

    // Object-like macro: no arguments expected
    if (macro.params.empty()) {
        parser.advance(); // consume macro name (no args to collect)
        return true;
    }

    // Function-like macro: expect '(' after the macro name
    parser.advance(); // consume macro name

    if (parser.current_.type != TokenType::LParen) {
        g_error_reporter.report_error(
            parser.current_.loc,
            "expected '(' after macro name '" + macro.name + "'"
        );
        consume_to_eol();
        return false;
    }

    parser.advance(); // consume '('

    // Special case: empty argument list "FOO()"
    if (parser.current_.type == TokenType::RParen) {
        parser.advance(); // consume ')'
        return true; // args is empty; caller will check arity
    }

    // Parse each argument
    for (std::size_t i = 0; i < macro.params.size(); ++i) {
        std::vector<Token> arg_tokens;
        int paren_depth = 0;

        while (true) {
            if (parser.current_.type == TokenType::EndOfInput ||
                    parser.current_.type == TokenType::EndOfLine) {
                g_error_reporter.report_error(
                    parser.current_.loc,
                    "unterminated macro argument list for '" + macro.name + "'"
                );
                consume_to_eol();
                return false;
            }

            // Stop at comma at depth 0 (end of this argument)
            if (parser.current_.is_comma() && paren_depth == 0) {
                break;
            }

            // Stop at ')' at depth 0 (end of argument list)
            if (parser.current_.type == TokenType::RParen && paren_depth == 0) {
                break;
            }

            // Track nested parentheses (allows nested macro calls)
            if (parser.current_.type == TokenType::LParen) {
                paren_depth++;
            }
            else if (parser.current_.type == TokenType::RParen) {
                paren_depth--;
                if (paren_depth < 0) {
                    g_error_reporter.report_error(
                        parser.current_.loc,
                        "unexpected ')' in macro argument list"
                    );
                    consume_to_eol();
                    return false;
                }
            }

            arg_tokens.push_back(parser.current_);
            parser.advance();
        }

        args.push_back(std::move(arg_tokens));

        // If this was the last expected argument, break
        if (i + 1 == macro.params.size()) {
            break;
        }

        // Expect comma between arguments
        if (!parser.current_.is_comma()) {
            g_error_reporter.report_error(
                parser.current_.loc,
                "expected ',' in macro argument list"
            );
            consume_to_eol();
            return false;
        }

        parser.advance(); // consume comma
    }

    // After parsing expected args, expect ')'
    if (parser.current_.type != TokenType::RParen) {
        g_error_reporter.report_error(
            parser.current_.loc,
            "expected ')' at end of macro call, found '" + parser.current_.text + "'"
        );
        consume_to_eol();
        return false;
    }

    parser.advance(); // consume ')'
    return true;
}

bool MacroExpander::is_builtin_name(const std::string& name) {
    return kBuiltins.find(name) != kBuiltins.end();
}

void MacroExpander::check_struct_stack() const {
    for (const BuiltinStructLevel& level : struct_stack_) {
        switch (level.type) {
        case BuiltinStruct::IF:
            g_error_reporter.report_error(
                level.loc,
                "if without matching endif"
            );
            break;
        case BuiltinStruct::ELSE:
            g_error_reporter.report_error(
                level.loc,
                "else without matching endif"
            );
            break;
        case BuiltinStruct::WHILE:
            g_error_reporter.report_error(
                level.loc,
                "while without matching endwhile"
            );
            break;
        case BuiltinStruct::REPEAT:
            g_error_reporter.report_error(
                level.loc,
                "repeat without matching endrepeat"
            );
            break;
        default:
            break;
        }
    }
}

bool MacroExpander::handle_alloc_cell(Parser& parser, const Token& tok) {
    // Expect alloc_cell(<ident>)
    Macro fake;
    fake.name = tok.text;
    fake.params = { "name" };
    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported; handled
    }

    if (args.size() != 1 || args[0].size() != 1 || args[0][0].type != TokenType::Identifier) {
        g_error_reporter.report_error(tok.loc, "alloc_cell expects one identifier");
        return true; // handled
    }

    const std::string macro_name = args[0][0].text;
    int addr = parser.output().alloc_cells(1);
    Macro m;
    m.name = macro_name;
    m.loc = tok.loc;
    m.body = { Token::make_int(addr, tok.loc) };
    g_macro_table.define(m);

    // output code to clear the allocated cell
    TokenScanner scanner;
    std::string mock_filename = "(alloc_cell)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(addr) + " [-] }",
            mock_filename));
    return true;
}

bool MacroExpander::handle_free_cell(Parser& parser, const Token& tok) {
    // Expect free_cell(<ident>)
    Macro fake;
    fake.name = tok.text;
    fake.params = { "name" };
    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported; handled
    }

    if (args.size() != 1 || args[0].size() != 1 || args[0][0].type != TokenType::Identifier) {
        g_error_reporter.report_error(tok.loc, "free_cell expects one identifier");
        return true;
    }

    const std::string macro_name = args[0][0].text;
    const Macro* m = g_macro_table.lookup(macro_name);
    if (!m) {
        g_error_reporter.report_error(tok.loc, "free_cell: macro '" + macro_name + "' not defined");
        return true;
    }
    if (!m->params.empty() || m->body.size() != 1 || m->body[0].type != TokenType::Integer) {
        g_error_reporter.report_error(tok.loc, "free_cell: '" + macro_name + "' is not an alloc_cell result");
        return true;
    }

    int addr = m->body[0].int_value;
    parser.output().free_cells(addr);
    g_macro_table.undef(macro_name);

    // output code to clear the freed cell
    TokenScanner scanner;
    std::string mock_filename = "(free_cell)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(addr) + " [-] }",
            mock_filename));

    return true;
}

bool MacroExpander::handle_clear(Parser& parser, const Token& tok) {
    // clear(<expr>) -> emits: { >expr-value [-] }
    Macro fake;
    fake.name = tok.text;
    fake.params = { "expr" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 1) {
        g_error_reporter.report_error(tok.loc, "clear expects one argument");
        return true;
    }

    // Evaluate the expression argument
    ArrayTokenSource source(args[0]);
    ExpressionParser expr(source, /*undefined_as_zero=*/false);
    int value = expr.parse_expression();

    // Build expansion: { > value [-] }
    TokenScanner scanner;
    std::string mock_filename = "(clear)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(value) + " [-] }",
            mock_filename));

    return true;
}

bool MacroExpander::handle_set(Parser& parser, const Token& tok) {
    // set(a, b) -> emits: { >a [-] +b }
    Macro fake;
    fake.name = tok.text;
    fake.params = { "a", "b" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 2) {
        g_error_reporter.report_error(tok.loc, "set expects two arguments");
        return true;
    }

    // Evaluate both expressions
    ArrayTokenSource src_a(args[0]);
    ExpressionParser expr_a(src_a, /*undefined_as_zero=*/false);
    int a_val = expr_a.parse_expression();

    ArrayTokenSource src_b(args[1]);
    ExpressionParser expr_b(src_b, /*undefined_as_zero=*/false);
    int b_val = expr_b.parse_expression();

    // Build expansion: { >a_val [-] +b_val }
    TokenScanner scanner;
    std::string mock_filename = "(set)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(a_val) + " [-] +" + std::to_string(b_val) + " }",
            mock_filename));

    return true;
}

bool MacroExpander::handle_move(Parser& parser, const Token& tok) {
    // move(a, b) -> emits: { >b [-] >a [ - >b + >a ] }
    Macro fake;
    fake.name   = tok.text;
    fake.params = { "a", "b" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 2) {
        g_error_reporter.report_error(tok.loc, "move expects two arguments");
        return true;
    }

    ArrayTokenSource src_a(args[0]);
    ExpressionParser expr_a(src_a, /*undefined_as_zero=*/false);
    int a_val = expr_a.parse_expression();

    ArrayTokenSource src_b(args[1]);
    ExpressionParser expr_b(src_b, /*undefined_as_zero=*/false);
    int b_val = expr_b.parse_expression();

    TokenScanner scanner;
    std::string mock_filename = "(move)";
    // { >b [-] >a [ - >b + >a ] }
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(b_val) +
            " [-] >" + std::to_string(a_val) +
            " [ - >" + std::to_string(b_val) +
            " + >" + std::to_string(a_val) + " ] }",
            mock_filename));
    return true;
}

bool MacroExpander::handle_copy(Parser& parser, const Token& tok) {
    // copy(a, b) -> { alloc_cell(T) >b [-] >a [ - >b + >T + >a ] >T [ - >a + >T ] free_cell(T) }
    Macro fake;
    fake.name   = tok.text;
    fake.params = { "a", "b" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 2) {
        g_error_reporter.report_error(tok.loc, "copy expects two arguments");
        return true;
    }

    ArrayTokenSource src_a(args[0]);
    ExpressionParser expr_a(src_a, /*undefined_as_zero=*/false);
    int a_val = expr_a.parse_expression();

    ArrayTokenSource src_b(args[1]);
    ExpressionParser expr_b(src_b, /*undefined_as_zero=*/false);
    int b_val = expr_b.parse_expression();

    std::string temp_name = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(copy)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + temp_name + ")"
            " >" + std::to_string(b_val) + " [-]"
            " >" + std::to_string(a_val) +
            " [ - >" + std::to_string(b_val) +
            " + >" + temp_name +
            " + >" + std::to_string(a_val) + " ]"
            " >" + temp_name +
            " [ - >" + std::to_string(a_val) +
            " + >" + temp_name + " ]"
            " free_cell(" + temp_name + ") }",
            mock_filename));
    return true;
}

bool MacroExpander::handle_not(Parser& parser, const Token& tok) {
    // not(expr) : sets cell to 1 if expr == 0, else 0
    Macro fake;
    fake.name = tok.text;
    fake.params = { "expr" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 1) {
        g_error_reporter.report_error(tok.loc, "not expects one argument");
        return true;
    }

    ArrayTokenSource src(args[0]);
    ExpressionParser expr(src, /*undefined_as_zero=*/false);
    int X = expr.parse_expression();

    std::string T = make_temp_name();
    std::string F = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(not)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate T and F
            "{ alloc_cell(" + T + ") "
            "  alloc_cell(" + F + ") "
            // Move X to T destoying X
            "  move(" + std::to_string(X) + ", " + T + ") "
            // Initialize result X = 1, flag F = 1
            "  >" + std::to_string(X) + " + "
            "  >" + F + " + "
            // Loop while T > 0
            "  >" + T + " "
            "  [ "                  // while T != 0
            "    - "                // T--
            "    >" + F + " [ "     //   if F != 0
            "         - "           //     F-- (so this runs only once)
            "         >" + std::to_string(X) + " - "    //     X-- (1 -> 0)
            "         >" + F + " "  //     back to F
            "       ] "             //   end if
            "    >" + T + " "       //   back to T
            "  ] "              // end while
            // Now T = 0, F = 0, X = !original (0 -> 1, >0 -> 0)
            "  free_cell(" + T + ") "
            "  free_cell(" + F + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_and(Parser& parser, const Token& tok) {
    // and(expr_a, expr_b) : sets expr_a cell to 1 if both expr_a and expr_b are non-zero
    Macro fake;
    fake.name = tok.text;
    fake.params = { "expr_a", "expr_b" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 2) {
        g_error_reporter.report_error(tok.loc, "and expects two arguments");
        return true;
    }

    ArrayTokenSource src_a(args[0]);
    ExpressionParser expr_a(src_a, /*undefined_as_zero=*/false);
    int a_val = expr_a.parse_expression();

    ArrayTokenSource src_b(args[1]);
    ExpressionParser expr_b(src_b, /*undefined_as_zero=*/false);
    int b_val = expr_b.parse_expression();

    std::string temp_a = make_temp_name();
    std::string temp_b = make_temp_name();
    std::string temp_r = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(and)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + temp_a + ")"
            "  alloc_cell(" + temp_b + ")"
            "  alloc_cell(" + temp_r + ")"
            // booleanize a_val to temp_a, destroys a_val
            "  move(" + std::to_string(a_val) + ", " + temp_a + ") "
            "  not(" + temp_a + ") "
            "  not(" + temp_a + ") "
            // booleanize b_val to temp_b, keeps b_val intact
            "  copy(" + std::to_string(b_val) + ", " + temp_b + ") "
            "  not(" + temp_b + ") "
            "  not(" + temp_b + ") "
            // if temp_a==1 move temp_b to temp_r, else temp_r = 0
            "  >" + temp_a + " [ - move(" + temp_b + ", " + temp_r + ") ] "
            // move temp_r to a_val
            "  move(" + temp_r + ", " + std::to_string(a_val) + ") "
            // free temps
            "  free_cell(" + temp_a + ") "
            "  free_cell(" + temp_b + ") "
            "  free_cell(" + temp_r + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_or(Parser& parser, const Token& tok) {
    // or(expr_a, expr_b) : sets expr_a cell to 1 if either expr_a or expr_b are non-zero
    Macro fake;
    fake.name = tok.text;
    fake.params = { "expr_a", "expr_b" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 2) {
        g_error_reporter.report_error(tok.loc, "or expects two arguments");
        return true;
    }

    ArrayTokenSource src_a(args[0]);
    ExpressionParser expr_a(src_a, /*undefined_as_zero=*/false);
    int a_val = expr_a.parse_expression();

    ArrayTokenSource src_b(args[1]);
    ExpressionParser expr_b(src_b, /*undefined_as_zero=*/false);
    int b_val = expr_b.parse_expression();

    std::string temp_a = make_temp_name();
    std::string temp_b = make_temp_name();
    std::string temp_r = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(or)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + temp_a + ")"
            "  alloc_cell(" + temp_b + ")"
            "  alloc_cell(" + temp_r + ")"
            // booleanize a_val to temp_a, destroys a_val
            "  move(" + std::to_string(a_val) + ", " + temp_a + ") "
            "  not(" + temp_a + ") "
            "  not(" + temp_a + ") "
            // booleanize b_val to temp_b, keeps b_val intact
            "  copy(" + std::to_string(b_val) + ", " + temp_b + ") "
            "  not(" + temp_b + ") "
            "  not(" + temp_b + ") "
            // add temp_a and temp_b to temp_r (result = 0, 1 or 2)
            "  >" + temp_a + " [ - >" + temp_r + " + >" + temp_a + " ] "
            "  >" + temp_b + " [ - >" + temp_r + " + >" + temp_b + " ] "
            // booleanize temp_r to 0 or 1
            "  not(" + temp_r + ") "
            "  not(" + temp_r + ") "
            // move temp_r to a_val
            "  move(" + temp_r + ", " + std::to_string(a_val) + ") "
            // free temps
            "  free_cell(" + temp_a + ") "
            "  free_cell(" + temp_b + ") "
            "  free_cell(" + temp_r + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_xor(Parser& parser, const Token& tok) {
    // xor(expr_a, expr_b) : sets expr_a cell to 1 if exactly one of expr_a or expr_b are non-zero
    Macro fake;
    fake.name = tok.text;
    fake.params = { "expr_a", "expr_b" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 2) {
        g_error_reporter.report_error(tok.loc, "xor expects two arguments");
        return true;
    }

    ArrayTokenSource src_a(args[0]);
    ExpressionParser expr_a(src_a, /*undefined_as_zero=*/false);
    int a_val = expr_a.parse_expression();

    ArrayTokenSource src_b(args[1]);
    ExpressionParser expr_b(src_b, /*undefined_as_zero=*/false);
    int b_val = expr_b.parse_expression();

    std::string temp_1 = make_temp_name();
    std::string temp_2 = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(xor)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + temp_1 + ")"
            "  alloc_cell(" + temp_2 + ")"
            // Compute A_OR_B into T1
            "  copy(" + std::to_string(a_val) + ", " + temp_1 + ") "
            "  or(" + temp_1 + ", " + std::to_string(b_val) + ") "
            // Compute A_AND_B into T2
            "  copy(" + std::to_string(a_val) + ", " + temp_2 + ") "
            "  and(" + temp_2 + ", " + std::to_string(b_val) + ") "
            // NOT(T2)
            "  not(" + temp_2 + ") "
            // XOR = (A OR B) AND NOT(A AND B)
            "  copy(" + temp_1 + ", " + std::to_string(a_val) + ") "
            "  and(" + std::to_string(a_val) + ", " + temp_2 + ") "
            // free temps
            "  free_cell(" + temp_1 + ") "
            "  free_cell(" + temp_2 + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_if(Parser& parser, const Token& tok) {
    // if(cond) / else / endif
    Macro fake;
    fake.name = tok.text;
    fake.params = { "expr" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 1) {
        g_error_reporter.report_error(tok.loc, "if expects one argument");
        return true;
    }

    // Evaluate the expression argument
    ArrayTokenSource source(args[0]);
    ExpressionParser expr(source, /*undefined_as_zero=*/false);
    int cond = expr.parse_expression();

    // create the If-stack entry
    BuiltinStructLevel level;
    level.type = BuiltinStruct::IF;
    level.loc = tok.loc;
    level.temp_if = make_temp_name();
    level.temp_else = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(if)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate temp variables
            "{ alloc_cell(" + level.temp_if + ") "
            "  alloc_cell(" + level.temp_else + ") "
            // copy cond to temp_else and negate it
            "  copy(" + std::to_string(cond) + ", " + level.temp_else + ") "
            "  not(" + level.temp_else + ")"
            // copy temp_else to temp_if and negate it
            "  copy(" + level.temp_else + ", " + level.temp_if + ") "
            "  not(" + level.temp_if + ") "
            // enter the IF branch if temp_if == 1
            "  >" + level.temp_if + " "
            "  [ {",
            mock_filename));
    struct_stack_.push_back(std::move(level));
    return true;
}

bool MacroExpander::handle_else(Parser& parser, const Token& tok) {
    parser.advance(); // consume else
    if (struct_stack_.empty()) {
        g_error_reporter.report_error(tok.loc, "else without matching if");
        return true;
    }

    BuiltinStructLevel& level = struct_stack_.back();
    if (level.type != BuiltinStruct::IF) {
        g_error_reporter.report_error(tok.loc, "else without if");
        return true;
    }
    level.type = BuiltinStruct::ELSE;

    TokenScanner scanner;
    std::string mock_filename = "(else)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // close previous IF branch
            "  } - ] "
            // enter the ELSE branch if temp_else == 1
            "  >" + level.temp_else + " "
            "  [ {",
            mock_filename));
    return true;
}

bool MacroExpander::handle_endif(Parser& parser, const Token& tok) {
    parser.advance(); // consume endif
    if (struct_stack_.empty()) {
        g_error_reporter.report_error(tok.loc, "endif without matching if");
        return true;
    }

    BuiltinStructLevel& level = struct_stack_.back();
    if (level.type != BuiltinStruct::IF &&
            level.type != BuiltinStruct::ELSE) {
        g_error_reporter.report_error(tok.loc, "endif without if");
        return true;
    }

    // Close the current IF branch
    TokenScanner scanner;
    std::string mock_filename = "(endif)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // close previous IF/ELSE branch
            "  } - ] "
            // release temp variables
            "  free_cell(" + level.temp_if + ") "
            "  free_cell(" + level.temp_else + ") "
            "}",
            mock_filename));

    struct_stack_.pop_back();
    return true;
}

bool MacroExpander::handle_while(Parser& parser, const Token& tok) {
    // while(cond) / endwhile
    Macro fake;
    fake.name = tok.text;
    fake.params = { "expr" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 1) {
        g_error_reporter.report_error(tok.loc, "while expects one argument");
        return true;
    }

    // Evaluate the expression argument
    ArrayTokenSource source(args[0]);
    ExpressionParser expr(source, /*undefined_as_zero=*/false);
    int cond = expr.parse_expression();

    // create the While-stack entry
    BuiltinStructLevel level;
    level.type = BuiltinStruct::WHILE;
    level.loc = tok.loc;
    level.temp_if = make_temp_name();
    level.cond = cond;

    TokenScanner scanner;
    std::string mock_filename = "(while)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate temp variable
            "{ alloc_cell(" + level.temp_if + ") "
            // copy cond to temp_if and negate it twice
            "  copy(" + std::to_string(level.cond) + ", "
            + level.temp_if + ") "
            "  not(" + level.temp_if + ")"
            "  not(" + level.temp_if + ")"
            // enter the WHILE branch if temp_if == 1
            "  >" + level.temp_if + " "
            "  [ {",
            mock_filename));
    struct_stack_.push_back(std::move(level));
    return true;
}

bool MacroExpander::handle_endwhile(Parser& parser, const Token& tok) {
    parser.advance(); // consume endwhile
    if (struct_stack_.empty()) {
        g_error_reporter.report_error(tok.loc, "endwhile without matching while");
        return true;
    }
    BuiltinStructLevel& level = struct_stack_.back();
    if (level.type != BuiltinStruct::WHILE) {
        g_error_reporter.report_error(tok.loc, "endwhile without matching while");
        return true;
    }

    // Close the current WHILE branch
    TokenScanner scanner;
    std::string mock_filename = "(endwhile)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // close WHILE brace
            "  } "
            // copy cond to temp_if and negate it twice
            "  copy(" + std::to_string(level.cond) + ", "
            + level.temp_if + ") "
            "  not(" + level.temp_if + ")"
            "  not(" + level.temp_if + ")"
            // re-enter the WHILE branch if temp_if == 1
            "  >" + level.temp_if + " "
            "  ] "
            // release temp variable
            "  free_cell(" + level.temp_if + ") "
            "}",
            mock_filename));
    struct_stack_.pop_back();

    return false;
}

bool MacroExpander::handle_repeat(Parser& parser, const Token& tok) {
    // repeat(count) / endrepeat
    Macro fake;
    fake.name = tok.text;
    fake.params = { "expr" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return true; // error already reported
    }

    if (args.size() != 1) {
        g_error_reporter.report_error(tok.loc, "repeat expects one argument");
        return true;
    }

    // Evaluate the expression argument
    ArrayTokenSource source(args[0]);
    ExpressionParser expr(source, /*undefined_as_zero=*/false);
    int count = expr.parse_expression();

    // create the Repeat-stack entry
    BuiltinStructLevel level;
    level.type = BuiltinStruct::REPEAT;
    level.loc = tok.loc;

    TokenScanner scanner;
    std::string mock_filename = "(repeat)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // enter the REPEAT branch if count > 0
            "{ >" + std::to_string(count) + " [ { ",
            mock_filename));
    struct_stack_.push_back(std::move(level));
    return true;
}

bool MacroExpander::handle_endrepeat(Parser& parser, const Token& tok) {
    parser.advance(); // consume endrepeat
    if (struct_stack_.empty()) {
        g_error_reporter.report_error(tok.loc, "endrepeat without matching repeat");
        return true;
    }
    BuiltinStructLevel& level = struct_stack_.back();
    if (level.type != BuiltinStruct::REPEAT) {
        g_error_reporter.report_error(tok.loc, "endrepeat without matching repeat");
        return true;
    }

    // Close the current REPEAT branch
    TokenScanner scanner;
    std::string mock_filename = "(endrepeat)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // decrement count and re-enter REPEAT branch if count > 0
            " } - ] }",
            mock_filename));
    struct_stack_.pop_back();

    return true;
}

std::vector<Token> MacroExpander::substitute_body(const Macro& macro,
        const std::vector<std::vector<Token>>& args) {
    std::vector<Token> result;

    for (const Token& tok : macro.body) {
        // Only identifiers can be parameters
        if (tok.type == TokenType::Identifier) {
            // Check if this identifier matches a formal parameter
            for (std::size_t i = 0; i < macro.params.size(); ++i) {
                if (tok.text == macro.params[i]) {
                    // Splice in the actual argument tokens
                    for (const Token& arg_tok : args[i]) {
                        result.push_back(arg_tok);
                    }
                    goto next_token;
                }
            }
        }

        // Not a parameter --> copy token as-is
        result.push_back(tok);

next_token:
        continue;
    }

    return result;
}

bool is_reserved_keyword(const std::string& name) {
    return name == "if" ||
           name == "else" ||
           name == "endif" ||
           name == "elsif" ||
           name == "include" ||
           name == "define" ||
           name == "undef" ||
           MacroExpander::is_builtin_name(name);
}

std::string make_temp_name() {
    static int s_temp_counter = 0;
    std::string temp_name = "_T" + std::to_string(++s_temp_counter);
    return temp_name;
}
