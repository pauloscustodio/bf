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
    { "alloc_cell",   &MacroExpander::handle_alloc_cell   },
    { "alloc_cell16", &MacroExpander::handle_alloc_cell16 },
    { "free_cell",    &MacroExpander::handle_free_cell    },
    { "free_cell16",  &MacroExpander::handle_free_cell16  },
    { "clear",        &MacroExpander::handle_clear        },
    { "clear16",      &MacroExpander::handle_clear16      },
    { "set",          &MacroExpander::handle_set          },
    { "set16",        &MacroExpander::handle_set16        },
    { "move",         &MacroExpander::handle_move         },
    { "move16",       &MacroExpander::handle_move16       },
    { "copy",         &MacroExpander::handle_copy         },
    { "copy16",       &MacroExpander::handle_copy16       },
    { "not",          &MacroExpander::handle_not          },
    { "not16",        &MacroExpander::handle_not16        },
    { "and",          &MacroExpander::handle_and          },
    { "and16",        &MacroExpander::handle_and16        },
    { "or",           &MacroExpander::handle_or           },
    { "or16",         &MacroExpander::handle_or16         },
    { "xor",          &MacroExpander::handle_xor          },
    { "xor16",        &MacroExpander::handle_xor16        },
    { "add",          &MacroExpander::handle_add          },
    { "add16",        &MacroExpander::handle_add16        },
    { "sub",          &MacroExpander::handle_sub          },
    { "sub16",        &MacroExpander::handle_sub16        },
    { "mul",          &MacroExpander::handle_mul          },
    { "mul16",        &MacroExpander::handle_mul16        },
    { "div",          &MacroExpander::handle_div          },
    { "div16",        &MacroExpander::handle_div16        },
    { "mod",          &MacroExpander::handle_mod          },
    { "mod16",        &MacroExpander::handle_mod16        },
    { "eq",           &MacroExpander::handle_eq           },
    { "eq16",         &MacroExpander::handle_eq16         },
    { "ne",           &MacroExpander::handle_ne           },
    { "ne16",         &MacroExpander::handle_ne16         },
    { "lt",           &MacroExpander::handle_lt           },
    { "lt16",         &MacroExpander::handle_lt16         },
    { "gt",           &MacroExpander::handle_gt           },
    { "gt16",         &MacroExpander::handle_gt16         },
    { "le",           &MacroExpander::handle_le           },
    { "le16",         &MacroExpander::handle_le16         },
    { "ge",           &MacroExpander::handle_ge           },
    { "ge16",         &MacroExpander::handle_ge16         },
    { "shr",          &MacroExpander::handle_shr          },
    { "shr16",        &MacroExpander::handle_shr16        },
    { "shl",          &MacroExpander::handle_shl          },
    { "shl16",        &MacroExpander::handle_shl16        },
    { "if",           &MacroExpander::handle_if           },
    { "else",         &MacroExpander::handle_else         },
    { "endif",        &MacroExpander::handle_endif        },
    { "while",        &MacroExpander::handle_while        },
    { "endwhile",     &MacroExpander::handle_endwhile     },
    { "repeat",       &MacroExpander::handle_repeat       },
    { "endrepeat",    &MacroExpander::handle_endrepeat    },
};

void MacroTable::clear() {
    table_.clear();
}

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
    std::string macro_name;
    if (!parse_ident_arg(parser, tok, macro_name)) {
        return true; // error already reported
    }

    int addr = parser.output().alloc_cells(1);
    Macro m;
    m.name = macro_name;
    m.loc = tok.loc;
    m.body = { Token::make_int(addr, tok.loc) };
    g_macro_table.define(m);

    TokenScanner scanner;
    std::string mock_filename = "(alloc_cell)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(addr) + " [-] }",
            mock_filename));
    return true;
}

bool MacroExpander::handle_alloc_cell16(Parser& parser, const Token& tok) {
    std::string macro_name;
    if (!parse_ident_arg(parser, tok, macro_name)) {
        return true; // error already reported
    }

    int addr = parser.output().alloc_cells(2);
    Macro m;
    m.name = macro_name;
    m.loc = tok.loc;
    m.body = { Token::make_int(addr, tok.loc) };
    g_macro_table.define(m);

    TokenScanner scanner;
    std::string mock_filename = "(alloc_cell16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(addr) + " [-] "
            "  >" + std::to_string(addr + 1) + " [-] "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_free_cell(Parser& parser, const Token& tok) {
    std::string macro_name;
    if (!parse_ident_arg(parser, tok, macro_name)) {
        return true; // error already reported
    }

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

    TokenScanner scanner;
    std::string mock_filename = "(free_cell)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(addr) + " [-] }",
            mock_filename));

    return true;
}

bool MacroExpander::handle_free_cell16(Parser& parser, const Token& tok) {
    std::string macro_name;
    if (!parse_ident_arg(parser, tok, macro_name)) {
        return true; // error already reported
    }

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

    TokenScanner scanner;
    std::string mock_filename = "(free_cell16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(addr) + " [-] "
            "  >" + std::to_string(addr + 1) + " [-] "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_clear(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int value = vals[0];

    TokenScanner scanner;
    std::string mock_filename = "(clear)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(value) + " [-] }",
            mock_filename));

    return true;
}

bool MacroExpander::handle_clear16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int value = vals[0];

    TokenScanner scanner;
    std::string mock_filename = "(clear16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(value) + " [-] "
            "  >" + std::to_string(value + 1) + " [-] "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_set(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(set)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(a) + " [-] +" + std::to_string(b) + " }",
            mock_filename));

    return true;
}

bool MacroExpander::handle_set16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];
    int b_low = b & 0xFF;
    int b_high = (b >> 8) & 0xFF;

    TokenScanner scanner;
    std::string mock_filename = "(set)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(a) + " [-] +" + std::to_string(b_low) +
            "  >" + std::to_string(a + 1) + " [-] +" + std::to_string(b_high) +
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_move(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(move)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(b) +
            " [-] >" + std::to_string(a) +
            " [ - >" + std::to_string(b) +
            " + >" + std::to_string(a) + " ] }",
            mock_filename));
    return true;
}

bool MacroExpander::handle_move16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(move)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "move(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "move(" + std::to_string(a + 1) + ", " + std::to_string(b + 1) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_copy(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_name = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(copy)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + t_name + ")"
            " >" + std::to_string(b) + " [-]"
            " >" + std::to_string(a) +
            " [ - >" + std::to_string(b) +
            " + >" + t_name +
            " + >" + std::to_string(a) + " ]"
            " >" + t_name +
            " [ - >" + std::to_string(a) +
            " + >" + t_name + " ]"
            " free_cell(" + t_name + ") }",
            mock_filename));
    return true;
}

bool MacroExpander::handle_copy16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(move)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "copy(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "copy(" + std::to_string(a + 1) + ", " + std::to_string(b + 1) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_not(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int X = vals[0];

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

bool MacroExpander::handle_not16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int a = vals[0];

    std::string T1 = make_temp_name();
    std::string T2 = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(not16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T1 + ") "
            "  alloc_cell(" + T2 + ") "
            // T1 = (a_lo == 0)
            "  copy(" + std::to_string(a) + ", " + T1 + ") "
            "  not(" + T1 + ") "
            // T2 = (a_hi == 0)
            "  copy(" + std::to_string(a + 1) + ", " + T2 + ") "
            "  not(" + T2 + ") "
            // T1 = T1 AND T2
            "  and(" + T1 + ", " + T2 + ") "
            // Write result back into a (16-bit)
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell(" + T1 + ") "
            "  free_cell(" + T2 + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_and(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_a = make_temp_name();
    std::string t_b = make_temp_name();
    std::string t_r = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(and)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + t_a + ")"
            "  alloc_cell(" + t_b + ")"
            "  alloc_cell(" + t_r + ")"
            // booleanize a to t_a, destroys a
            "  move(" + std::to_string(a) + ", " + t_a + ") "
            "  not(" + t_a + ") "
            "  not(" + t_a + ") "
            // booleanize b to t_b, keeps b intact
            "  copy(" + std::to_string(b) + ", " + t_b + ") "
            "  not(" + t_b + ") "
            "  not(" + t_b + ") "
            // if t_a==1 move t_b to t_r, else t_r = 0
            "  >" + t_a + " [ - move(" + t_b + ", " + t_r + ") ] "
            // move t_r to a
            "  move(" + t_r + ", " + std::to_string(a) + ") "
            // free temps
            "  free_cell(" + t_a + ") "
            "  free_cell(" + t_b + ") "
            "  free_cell(" + t_r + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_and16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T1 = make_temp_name();
    std::string T2 = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(and16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T1 + ") "
            "  alloc_cell(" + T2 + ") "
            // T1 = (a_lo != 0) OR (a_hi != 0)
            " copy(" + std::to_string(a) + ", " + T1 + ") "
            " or(" + T1 + ", " + std::to_string(a + 1) + ") "
            // T2 = (b_lo != 0) OR (b_hi != 0)
            " copy(" + std::to_string(b) + ", " + T2 + ") "
            " or(" + T2 + ", " + std::to_string(b + 1) + ") "
            // T1 = T1 AND ((b_lo != 0) OR (b_hi != 0))
            " and(" + T1 + ", " + T2 + ") "
            // if T1 != 0 set a=1 else a=0
            " if(" + T1 + ") "
            "   set16(" + std::to_string(a) + ", 1) "
            " else "
            "   clear16(" + std::to_string(a) + ") "
            " endif "
            // free temps
            " free_cell(" + T1 + ") "
            " free_cell(" + T2 + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_or(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_a = make_temp_name();
    std::string t_b = make_temp_name();
    std::string t_r = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(or)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + t_a + ")"
            "  alloc_cell(" + t_b + ")"
            "  alloc_cell(" + t_r + ")"
            // booleanize a to t_a, destroys a
            "  move(" + std::to_string(a) + ", " + t_a + ") "
            "  not(" + t_a + ") "
            "  not(" + t_a + ") "
            // booleanize b to t_b, keeps b intact
            "  copy(" + std::to_string(b) + ", " + t_b + ") "
            "  not(" + t_b + ") "
            "  not(" + t_b + ") "
            // add t_a and t_b to t_r (result = 0, 1 or 2)
            "  >" + t_a + " [ - >" + t_r + " + >" + t_a + " ] "
            "  >" + t_b + " [ - >" + t_r + " + >" + t_b + " ] "
            // booleanize t_r to 0 or 1
            "  not(" + t_r + ") "
            "  not(" + t_r + ") "
            // move t_r to a
            "  move(" + t_r + ", " + std::to_string(a) + ") "
            // free temps
            "  free_cell(" + t_a + ") "
            "  free_cell(" + t_b + ") "
            "  free_cell(" + t_r + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_or16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(or16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T + ") "
            // T = a_lo OR a_hi
            "  copy(" + std::to_string(a) + ", " + T + ") "
            "  or(" + T + ", " + std::to_string(a + 1) + ") "
            // T = T OR b_lo
            "  or(" + T + ", " + std::to_string(b) + ") "
            // T = T OR b_hi
            "  or(" + T + ", " + std::to_string(b + 1) + ") "
            //  ---- write result back into a ----
            "  if(" + T + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell(" + T + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_xor(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T1 = make_temp_name();
    std::string T2 = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(xor)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T1 + ")"
            "  alloc_cell(" + T2 + ")"
            // Compute A_OR_B into T1
            "  copy(" + std::to_string(a) + ", " + T1 + ") "
            "  or(" + T1 + ", " + std::to_string(b) + ") "
            // Compute A_AND_B into T2
            "  copy(" + std::to_string(a) + ", " + T2 + ") "
            "  and(" + T2 + ", " + std::to_string(b) + ") "
            // NOT(T2)
            "  not(" + T2 + ") "
            // XOR = (A OR B) AND NOT(A AND B)
            "  copy(" + T1 + ", " + std::to_string(a) + ") "
            "  and(" + std::to_string(a) + ", " + T2 + ") "
            // free temps
            "  free_cell(" + T1 + ") "
            "  free_cell(" + T2 + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_xor16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T1 = make_temp_name();
    std::string T2 = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(xor16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // xor(a, b) = (a OR b) AND NOT(a AND b)
            "{ alloc_cell16(" + T1 + ") "
            "  alloc_cell16(" + T2 + ") "
            // T1 = a OR b
            "  copy16(" + std::to_string(a) + ", " + T1 + ") "
            "  or16(" + T1 + ", " + std::to_string(b) + ") "
            // T2 = NOT(a AND b)
            "  copy16(" + std::to_string(a) + ", " + T2 + ") "
            "  and16(" + T2 + ", " + std::to_string(b) + ") "
            "  not16(" + T2 + ") "
            // XOR = T1 AND T2
            "  and16(" + T1 + ", " + T2 + ") "
            //  ---- write result back into a ----
            "  if(" + T1 + ") "     // look at low byte of result
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell16(" + T1 + ") "
            "  free_cell16(" + T2 + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_add(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(add)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate T variable
            "{ alloc_cell(" + T + ") "
            // copy b to T
            "  copy(" + std::to_string(b) + ", " + T + ") "
            // add T to a
            "  >" + T +
            " [ - >" + std::to_string(a) + " + >" + T + " ] "
            // free T
            "  free_cell(" + T + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_add16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_old = make_temp_name();
    std::string t_carry = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(add16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + t_old + ") "
            "  alloc_cell(" + t_carry + ") "
            // ---- Step 1: save old low byte ----
            "  copy(" + std::to_string(a) + ", " + t_old + ") "
            // ---- Step 2: add low bytes ----
            "  add(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // ---- Step 3: detect carry ----
            // T_carry = new a_lo
            "  copy(" + std::to_string(a) + ", " + t_carry + ") "
            // carry = (a_lo < old)
            "  lt(" + t_carry + ", " + t_old + ") "
            // ---- Step 4: add high bytes ----
            "  add(" + std::to_string(a + 1) + ", " + std::to_string(b + 1) + ") "
            // ---- Step 4: add carry into high byte ----
            "  add(" + std::to_string(a + 1) + ", " + t_carry + ") "
            "  free_cell(" + t_old + ") "
            "  free_cell(" + t_carry + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_sub(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(sub)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate T variable
            "{ alloc_cell(" + T + ") "
            // copy b to T
            "  copy(" + std::to_string(b) + ", " + T + ") "
            // sub T from a
            "  >" + T +
            " [ - >" + std::to_string(a) + " - >" + T + " ] "
            // free T
            "  free_cell(" + T + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_sub16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_old = make_temp_name();
    std::string t_borrow = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(sub16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + t_old + ") "
            "  alloc_cell(" + t_borrow + ") "
            // ---- Step 1: save old low byte ----
            "  copy(" + std::to_string(a) + ", " + t_old + ") "
            // ---- Step 2: sub low bytes ----
            "  sub(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // ---- Step 3: detect borrow ----
            // T_borrow = new a_lo
            "  copy(" + std::to_string(a) + ", " + t_borrow + ") "
            // borrow = (a_lo > old)
            "  gt(" + t_borrow + ", " + t_old + ") "
            // ---- Step 4: sub high bytes ----
            "  sub(" + std::to_string(a + 1) + ", " + std::to_string(b + 1) + ") "
            // ---- Step 4: sub borrow into high byte ----
            "  sub(" + std::to_string(a + 1) + ", " + t_borrow + ") "
            "  free_cell(" + t_old + ") "
            "  free_cell(" + t_borrow + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_mul(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_res = make_temp_name();
    std::string T_b = make_temp_name();
    std::string T_tmp = make_temp_name();
    std::string T_one = make_temp_name();
    std::string T_two = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(mul)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T_res + ") "
            "  alloc_cell(" + T_b + ") "
            "  alloc_cell(" + T_tmp + ") "
            "  alloc_cell(" + T_one + ") >" + T_one + " + "
            "  alloc_cell(" + T_two + ") >" + T_two + " ++ "
            "  copy(" + std::to_string(b) + ", " + T_b + ") "
            "  while(" + T_b + ") "
            //   if (b is odd)
            "    copy(" + T_b + ", " + T_tmp + ") "
            "    mod(" + T_tmp + ", " + T_two + ") "
            "    if(" + T_tmp + ") "
            //     add a to result
            "      add(" + T_res + ", " + std::to_string(a) + ") "
            "    endif "
            //   b = b // 2
            "    shr(" + T_b + ", " + T_one + ") "
            //   a = a * 2
            "    shl(" + std::to_string(a) + ", " + T_one + ") "
            "  endwhile "
            // move result back to a
            "  move(" + T_res + ", " + std::to_string(a) + ") "
            // free T
            "  free_cell(" + T_res + ") "
            "  free_cell(" + T_b + ") "
            "  free_cell(" + T_tmp + ") "
            "  free_cell(" + T_one + ") "
            "  free_cell(" + T_two + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_mul16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_acc = make_temp_name();
    std::string T_mul = make_temp_name();
    std::string T_mcand = make_temp_name();
    std::string T_tmp = make_temp_name();
    std::string T_one = make_temp_name();
    std::string T_two = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(mul16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell16(" + T_acc + ") "
            "  alloc_cell16(" + T_mul + ") "
            "  alloc_cell16(" + T_mcand + ") "
            "  alloc_cell16(" + T_tmp + ") "
            "  alloc_cell16(" + T_one + ") >" + T_one + " + "
            "  alloc_cell16(" + T_two + ") >" + T_two + " ++ "

            "  clear16(" + T_acc + ") "
            "  copy16(" + std::to_string(a) + ", " + T_mcand + ") "
            "  copy16(" + std::to_string(b) + ", " + T_mul + ") "

            "  copy16(" + T_mul + ", " + T_tmp + ") "
            "  ge16(" + T_tmp + ", " + T_one + ") "
            "  while(" + T_tmp + ") "
            //   if (b is odd)
            "    copy16(" + T_mul + ", " + T_tmp + ") "
            "    mod16(" + T_tmp + ", " + T_two + ") "
            "    if(" + T_tmp + ") "
            //     add a to result
            "      add16(" + T_acc + ", " + T_mcand + ") "
            "    endif "
            //   b = b // 2
            "    shr16(" + T_mul + ", " + T_one + ") "
            //   a = a * 2
            "    shl16(" + T_mcand + ", " + T_one + ") "
            //   recompute loop condition
            "    copy16(" + T_mul + ", " + T_tmp + ") "
            "    ge16(" + T_tmp + ", " + T_one + ") "
            "  endwhile "
            // move result back to a
            "  move16(" + T_acc + ", " + std::to_string(a) + ") "
            // free T
            "  free_cell16(" + T_acc + ") "
            "  free_cell16(" + T_mul + ") "
            "  free_cell16(" + T_mcand + ") "
            "  free_cell16(" + T_tmp + ") "
            "  free_cell16(" + T_one + ") "
            "  free_cell16(" + T_two + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_div(Parser& parser, const Token& tok) {
    return handle_div_mod(parser, tok, /*return_remainder=*/false);
}

bool MacroExpander::handle_div16(Parser& parser, const Token& tok) {
    return handle_div16_mod16(parser, tok, /*return_remainder=*/false);
}

bool MacroExpander::handle_mod(Parser& parser, const Token& tok) {
    return handle_div_mod(parser, tok, /*return_remainder=*/true);
}

bool MacroExpander::handle_mod16(Parser& parser, const Token& tok) {
    return handle_div16_mod16(parser, tok, /*return_remainder=*/true);
}

bool MacroExpander::handle_div_mod(Parser& parser, const Token& tok,
                                   bool return_remainder) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_quot = make_temp_name();
    std::string T_rem = make_temp_name();
    std::string T_bit = make_temp_name();
    std::string T_tmp = make_temp_name();
    std::string T_one = make_temp_name();
    std::string T_seven = make_temp_name();
    std::string T_eight = make_temp_name();

    const std::string move_target = return_remainder ? T_rem : T_quot;
    const std::string mock_filename = return_remainder ? "(mod)" : "(div)";

    TokenScanner scanner;
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T_quot + ") "
            "  alloc_cell(" + T_rem + ") "
            "  alloc_cell(" + T_bit + ") "
            "  alloc_cell(" + T_tmp + ") "
            "  alloc_cell(" + T_one + ") >" + T_one + " + "
            "  alloc_cell(" + T_seven + ") >" + T_seven + " +7 "
            "  alloc_cell(" + T_eight + ") >" + T_eight + " +8 "
            "  if(" + std::to_string(b) + ") "
            "    repeat(" + T_eight + ") "
            "      copy(" + std::to_string(a) + ", " + T_bit + ") "
            "      shr(" + T_bit + ", " + T_seven + ") "
            "      shl(" + std::to_string(a) + ", " + T_one + ") "
            "      shl(" + T_rem + ", " + T_one + ") "
            "      add(" + T_rem + ", " + T_bit + ") "
            "      copy(" + T_rem + ", " + T_tmp + ") "
            "      ge(" + T_tmp + ", " + std::to_string(b) + ") "
            "      if(" + T_tmp + ") "
            "        sub(" + T_rem + ", " + std::to_string(b) + ") "
            "        shl(" + T_quot + ", " + T_one + ") "
            "        add(" + T_quot + ", " + T_one + ") "
            "      else "
            "        shl(" + T_quot + ", " + T_one + ") "
            "      endif "
            "    endrepeat "
            "    move(" + move_target + ", " + std::to_string(a) + ") "
            "  endif "
            "  free_cell(" + T_quot + ") "
            "  free_cell(" + T_rem + ") "
            "  free_cell(" + T_bit + ") "
            "  free_cell(" + T_tmp + ") "
            "  free_cell(" + T_one + ") "
            "  free_cell(" + T_seven + ") "
            "  free_cell(" + T_eight + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_div16_mod16(Parser& parser, const Token& tok,
                                       bool return_remainder) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_work = make_temp_name();
    std::string T_quot = make_temp_name();
    std::string T_scale = make_temp_name();
    std::string T_bit = make_temp_name();
    std::string T_tmp = make_temp_name();
    std::string T_cond = make_temp_name();
    std::string T_one = make_temp_name();

    const std::string move_target = return_remainder ? T_work : T_quot;
    const std::string mock_filename = return_remainder ? "(mod16)" : "(div16)";

    TokenScanner scanner;
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell16(" + T_work + ") "
            "  alloc_cell16(" + T_quot + ") "
            "  alloc_cell16(" + T_scale + ") "
            "  alloc_cell16(" + T_bit + ") "
            "  alloc_cell16(" + T_tmp + ") "
            "  alloc_cell16(" + T_cond + ") "
            "  alloc_cell16(" + T_one + ") >" + T_one + " + "
            // division by zero check
            "  copy16(" + std::to_string(b) + ", " + T_cond + ") "
            "  ge16(" + T_cond + ", " + T_one + ") "
            "  if(" + T_cond + ") "
            //   work = a
            "    copy16(" + std::to_string(a) + ", " + T_work + ") "
            //   while work >= b
            "    copy16(" + T_work + ", " + T_cond + ") "
            "    ge16(" + T_cond + ", " + std::to_string(b) + ") "
            "    while (" + T_cond + ") "
            //     scale = b
            "      copy16(" + std::to_string(b) + ", " + T_scale + ") "
            //     bit = 1
            "      clear16(" + T_bit + ") "
            "      add16(" + T_bit + ", " + T_one + ") "
            //     grow scale while (scale << 1) <= work
            "      copy16(" + T_scale + ", " + T_tmp + ") "
            "      shl16(" + T_tmp + ", " + T_one + ") "
            "      copy16(" + T_work + ", " + T_cond + ") "
            "      ge16(" + T_cond + ", " + T_tmp + ") "
            "      while (" + T_cond + ") "
            "        shl16(" + T_scale + ", " + T_one + ") "
            "        shl16(" + T_bit + ", " + T_one + ") "
            //       recompute loop condition
            "        copy16(" + T_scale + ", " + T_tmp + ") "
            "        shl16(" + T_tmp + ", " + T_one + ") "
            "        copy16(" + T_work + ", " + T_cond + ") "
            "        ge16(" + T_cond + ", " + T_tmp + ") "
            "      endwhile "
            //     subtract largest chunk
            "      sub16(" + T_work + ", " + T_scale + ") "
            //     accumulate quotient
            "      add16(" + T_quot + ", " + T_bit + ") "
            //     recompute loop condition
            "      copy16(" + T_work + ", " + T_cond + ") "
            "      ge16(" + T_cond + ", " + std::to_string(b) + ") "
            "    endwhile "
            //   return result
            "    move16(" + move_target + ", " + std::to_string(a) + ") "
            "  endif "
            // free cells
            "  free_cell16(" + T_work + ") "
            "  free_cell16(" + T_quot + ") "
            "  free_cell16(" + T_scale + ") "
            "  free_cell16(" + T_bit + ") "
            "  free_cell16(" + T_tmp + ") "
            "  free_cell16(" + T_cond + ") "
            "  free_cell16(" + T_one + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_eq(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(eq)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // a -= b
            "sub(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // not(a): 0 (equal) -> 1, non-zero (not equal) -> 0
            "not(" + std::to_string(a) + ") ",
            mock_filename));

    return true;
}

bool MacroExpander::handle_eq16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T1 = make_temp_name();
    std::string T2 = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(eq16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T1 + ") "
            "  alloc_cell(" + T2 + ") "
            // T1 = (a_lo == b_lo)
            "  copy(" + std::to_string(a) + ", " + T1 + ") "
            "  eq(" + T1 + ", " + std::to_string(b) + ") "
            // T2 = (a_hi == b_hi)
            "  copy(" + std::to_string(a + 1) + ", " + T2 + ") "
            "  eq(" + T2 + ", " + std::to_string(b + 1) + ") "
            // T1 = T1 AND T2
            "  and(" + T1 + ", " + T2 + ") "
            // copy the result back into a (16-bit)
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell(" + T1 + ") "
            "  free_cell(" + T2 + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_ne(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(ne)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // a == b
            "eq(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // not(a)
            "not(" + std::to_string(a) + ") ",
            mock_filename));

    return true;
}

bool MacroExpander::handle_ne16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(ne16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // a == b
            "eq16(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // not(a)
            "not16(" + std::to_string(a) + ") ",
            mock_filename));

    return true;
}

bool MacroExpander::handle_lt(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_a = make_temp_name();
    std::string t_b = make_temp_name();
    std::string t_a_and_b = make_temp_name();
    std::string temp_lt = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(lt)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell(" + t_a + ") "
            "  alloc_cell(" + t_b + ") "
            "  alloc_cell(" + t_a_and_b + ") "
            "  alloc_cell(" + temp_lt + ") "
            // copy a to t_a and b to t_b
            "  copy(" + std::to_string(a) + ", " + t_a + ") "
            "  copy(" + std::to_string(b) + ", " + t_b + ") "
            // while t_a > 0 and t_b > 0, decrement both
            "  copy(" + t_a + ", " + t_a_and_b + ") "
            "  and(" + t_a_and_b + ", " + t_b + ") "
            "  while(" + t_a_and_b + ") "
            "    >" + t_a + " - "
            "    >" + t_b + " - "
            "    copy(" + t_a + ", " + t_a_and_b + ") "
            "    and(" + t_a_and_b + ", " + t_b + ") "
            "  endwhile "
            // if t_a == 0 and t_b > 0, then a < b
            "  clear(" + std::to_string(a) + ") "
            "  copy(" + t_a + ", " + temp_lt + ") "
            "  not(" + temp_lt + ") "
            "  and(" + temp_lt + ", " + t_b + ") "
            "  if(" + temp_lt + ") "
            "    >" + std::to_string(a) + " + "
            "  endif "
            // free temp variables
            "  free_cell(" + t_a + ") "
            "  free_cell(" + t_b + ") "
            "  free_cell(" + t_a_and_b + ") "
            "  free_cell(" + temp_lt + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_lt16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T1 = make_temp_name();
    std::string T2 = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(lt16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T1 + ") "
            "  alloc_cell(" + T2 + ") "
            // T1 = (a_hi < b_hi)
            "  copy(" + std::to_string(a + 1) + ", " + T1 + ") "
            "  lt(" + T1 + ", " + std::to_string(b + 1) + ") "
            // T2 = (a_hi == b_hi)
            "  copy(" + std::to_string(a + 1) + ", " + T2 + ") "
            "  eq(" + T2 + ", " + std::to_string(b + 1) + ") "
            // If high bytes equal, refine result with low-byte comparison
            "  if(" + T2 + ") "
            // T1 = (a_lo < b_lo)
            "    copy(" + std::to_string(a) + ", " + T1 + ") "
            "    lt(" + T1 + ", " + std::to_string(b) + ") "
            "  endif "
            // copy the result back into a (16-bit)
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell(" + T1 + ") "
            "  free_cell(" + T2 + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_gt(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_a = make_temp_name();
    std::string t_b = make_temp_name();
    std::string t_a_and_b = make_temp_name();
    std::string t_gt = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(gt)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell(" + t_a + ") "
            "  alloc_cell(" + t_b + ") "
            "  alloc_cell(" + t_a_and_b + ") "
            "  alloc_cell(" + t_gt + ") "
            // copy a to t_a and b to t_b
            "  copy(" + std::to_string(a) + ", " + t_a + ") "
            "  copy(" + std::to_string(b) + ", " + t_b + ") "
            // while t_a > 0 and t_b > 0, decrement both
            "  copy(" + t_a + ", " + t_a_and_b + ") "
            "  and(" + t_a_and_b + ", " + t_b + ") "
            "  while(" + t_a_and_b + ") "
            "    >" + t_a + " - "
            "    >" + t_b + " - "
            "    copy(" + t_a + ", " + t_a_and_b + ") "
            "    and(" + t_a_and_b + ", " + t_b + ") "
            "  endwhile "
            // if t_b == 0 and t_a > 0, then a > b
            "  clear(" + std::to_string(a) + ") "
            "  copy(" + t_b + ", " + t_gt + ") "
            "  not(" + t_gt + ") "
            "  and(" + t_gt + ", " + t_a + ") "
            "  if(" + t_gt + ") "
            "    >" + std::to_string(a) + " + "
            "  endif "
            // free temp variables
            "  free_cell(" + t_a + ") "
            "  free_cell(" + t_b + ") "
            "  free_cell(" + t_a_and_b + ") "
            "  free_cell(" + t_gt + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_gt16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T1 = make_temp_name();
    std::string T2 = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(gt16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T1 + ") "
            "  alloc_cell(" + T2 + ") "
            // T1 = (a_hi > b_hi)
            "  copy(" + std::to_string(a + 1) + ", " + T1 + ") "
            "  gt(" + T1 + ", " + std::to_string(b + 1) + ") "
            // T2 = (a_hi == b_hi)
            "  copy(" + std::to_string(a + 1) + ", " + T2 + ") "
            "  eq(" + T2 + ", " + std::to_string(b + 1) + ") "
            // If high bytes equal, refine result with low-byte comparison
            "  if(" + T2 + ") "
            // T1 = (a_lo > b_lo)
            "    copy(" + std::to_string(a) + ", " + T1 + ") "
            "    gt(" + T1 + ", " + std::to_string(b) + ") "
            "  endif "
            // copy the result back into a (16-bit)
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell(" + T1 + ") "
            "  free_cell(" + T2 + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_le(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(le)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // (a <= b) is !(a > b)
            "gt(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "not(" + std::to_string(a) + ") ",
            mock_filename));

    return true;
}

bool MacroExpander::handle_le16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(le16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // (a <= b) is !(a > b)
            "gt16(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "not16(" + std::to_string(a) + ") ",
            mock_filename));

    return true;
}

bool MacroExpander::handle_ge(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(ge)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // (a >= b) is !(a < b)
            "lt(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "not(" + std::to_string(a) + ") ",
            mock_filename));

    return true;
}

bool MacroExpander::handle_ge16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(ge16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // (a >= b) is !(a < b)
            "lt16(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "not16(" + std::to_string(a) + ") ",
            mock_filename));

    return true;
}

bool MacroExpander::handle_shr(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_half = make_temp_name();
    std::string T_cmp = make_temp_name();
    std::string T_one = make_temp_name();
    std::string T_two = make_temp_name();
    std::string T_count = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(shr)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T_half + ") "
            "  alloc_cell(" + T_cmp + ") "
            "  alloc_cell(" + T_one + ") >" + T_one + " + "
            "  alloc_cell(" + T_two + ") >" + T_two + " ++ "
            "  alloc_cell(" + T_count + ") "
            // copy shift count to T_count
            "  copy(" + std::to_string(b) + ", " + T_count + ") "
            "  repeat(" + T_count + ") "
            //   while (a >= 2), shift right
            "    copy(" + std::to_string(a) + ", " + T_cmp + ") "
            "    ge(" + T_cmp + ", " + T_two + ") "
            "    while(" + T_cmp + ") "
            "      sub(" + std::to_string(a) + ", " + T_two + ") " // a -= 2
            "      add(" + T_half + ", " + T_one + ") " // half += 1
            //     recompute condition for next iteration
            "      copy(" + std::to_string(a) + ", " + T_cmp + ") "
            "      ge(" + T_cmp + ", " + T_two + ") "
            "    endwhile "
            //   set the result
            "    move(" + T_half + ", " + std::to_string(a) + ") "
            "  endrepeat "
            "  free_cell(" + T_half + ") "
            "  free_cell(" + T_cmp + ") "
            "  free_cell(" + T_one + ") "
            "  free_cell(" + T_two + ") "
            "  free_cell(" + T_count + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_shr16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_half = make_temp_name();
    std::string T_cmp = make_temp_name();
    std::string T_one = make_temp_name();
    std::string T_two = make_temp_name();
    std::string T_count = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(shr16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell16(" + T_half + ") "
            "  alloc_cell16(" + T_cmp + ") "
            "  alloc_cell16(" + T_one + ") >" + T_one + " + "
            "  alloc_cell16(" + T_two + ") >" + T_two + " ++ "
            "  alloc_cell16(" + T_count + ") "
            // copy shift count to T_count
            "  copy16(" + std::to_string(b) + ", " + T_count + ") "
            "  repeat(" + T_count + ") "
            //   while (a >= 2), shift right
            "    copy16(" + std::to_string(a) + ", " + T_cmp + ") "
            "    ge16(" + T_cmp + ", " + T_two + ") "
            "    while(" + T_cmp + ") "
            "      sub16(" + std::to_string(a) + ", " + T_two + ") " // a -= 2
            "      add16(" + T_half + ", " + T_one + ") " // half += 1
            //     recompute condition for next iteration
            "      copy16(" + std::to_string(a) + ", " + T_cmp + ") "
            "      ge16(" + T_cmp + ", " + T_two + ") "
            "    endwhile "
            //   set the result
            "    move16(" + T_half + ", " + std::to_string(a) + ") "
            "  endrepeat "
            "  free_cell16(" + T_half + ") "
            "  free_cell16(" + T_cmp + ") "
            "  free_cell16(" + T_one + ") "
            "  free_cell16(" + T_two + ") "
            "  free_cell16(" + T_count + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_shl(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_val = make_temp_name();
    std::string T_count = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(shl)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell(" + T_val + ") "
            "  alloc_cell(" + T_count + ") "
            // copy shift count to T_count
            "  copy(" + std::to_string(b) + ", " + T_count + ") "
            "  repeat(" + T_count + ") "
            //   duplicate a
            "    copy(" + std::to_string(a) + ", " + T_val + ") "
            "    add(" + std::to_string(a) + ", " + T_val + ") "
            "  endrepeat "
            "  free_cell(" + T_val + ") "
            "  free_cell(" + T_count + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_shl16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_val = make_temp_name();
    std::string T_count = make_temp_name();

    TokenScanner scanner;
    std::string mock_filename = "(shl16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell16(" + T_val + ") "
            "  alloc_cell16(" + T_count + ") "
            // copy shift count to T_count
            "  copy16(" + std::to_string(b) + ", " + T_count + ") "
            "  repeat(" + T_count + ") "
            //   duplicate a
            "    copy16(" + std::to_string(a) + ", " + T_val + ") "
            "    add16(" + std::to_string(a) + ", " + T_val + ") "
            "  endrepeat "
            "  free_cell16(" + T_val + ") "
            "  free_cell16(" + T_count + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_if(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int cond = vals[0];

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
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int cond = vals[0];

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
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int count = vals[0];

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

bool MacroExpander::parse_expr_args(Parser& parser,
                                    const Token& tok,
                                    const std::vector<std::string>& param_names,
                                    std::vector<int>& values) {
    // Save the original macro name before collect_args moves parser.current_
    const std::string macro_name = tok.text;

    Macro fake;
    fake.name = macro_name;
    fake.params = param_names;

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return false; // error already reported
    }

    if (args.size() != param_names.size()) {
        g_error_reporter.report_error(
            tok.loc,
            "macro '" + macro_name + "' expects " +
            std::to_string(param_names.size()) +
            (param_names.size() == 1 ? " argument" : " arguments")
        );
        return false;
    }

    values.clear();
    values.reserve(args.size());
    for (const auto& arg_tokens : args) {
        ArrayTokenSource source(arg_tokens);
        ExpressionParser expr(source, /*undefined_as_zero=*/false);
        values.push_back(expr.parse_expression());
    }
    return true;
}

bool MacroExpander::parse_ident_arg(Parser& parser,
                                    const Token& tok,
                                    std::string& ident_out) {
    // Save name before collect_args moves parser.current_
    const std::string macro_name = tok.text;

    Macro fake;
    fake.name = macro_name;
    fake.params = { "name" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return false; // error already reported
    }

    if (args.size() != 1 || args[0].size() != 1 ||
            args[0][0].type != TokenType::Identifier) {
        g_error_reporter.report_error(tok.loc,
                                      "macro '" + macro_name + "' expects one identifier");
        return false;
    }

    ident_out = args[0][0].text;
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

// temporary names generated for macro expansions
static int g_temp_counter = 0;

std::string make_temp_name() {
    return "_T" + std::to_string(++g_temp_counter);
}

void reset_temp_names() {
    g_temp_counter = 0;
}
