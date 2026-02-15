//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "expr.h"
#include "macros.h"
#include "parser.h"
#include <cassert>

MacroTable g_macro_table;

// Built-ins are now private to MacroExpander
const std::unordered_map<std::string, MacroExpander::BuiltinHandler> MacroExpander::kBuiltins = {
    { "alloc_cell8",        &MacroExpander::handle_alloc_cell8        },
    { "alloc_cell16",       &MacroExpander::handle_alloc_cell16       },
    { "free_cell8",         &MacroExpander::handle_free_cell8         },
    { "free_cell16",        &MacroExpander::handle_free_cell16        },
    { "clear8",             &MacroExpander::handle_clear8             },
    { "clear16",            &MacroExpander::handle_clear16            },
    { "set8",               &MacroExpander::handle_set8               },
    { "set16",              &MacroExpander::handle_set16              },
    { "move8",              &MacroExpander::handle_move8              },
    { "move16",             &MacroExpander::handle_move16             },
    { "copy8",              &MacroExpander::handle_copy8              },
    { "copy16",             &MacroExpander::handle_copy16             },
    { "not8",               &MacroExpander::handle_not8               },
    { "not16",              &MacroExpander::handle_not16              },
    { "and8",               &MacroExpander::handle_and8               },
    { "and16",              &MacroExpander::handle_and16              },
    { "or8",                &MacroExpander::handle_or8                },
    { "or16",               &MacroExpander::handle_or16               },
    { "xor8",               &MacroExpander::handle_xor8               },
    { "xor16",              &MacroExpander::handle_xor16              },
    { "add8",               &MacroExpander::handle_add8               },
    { "add16",              &MacroExpander::handle_add16              },
    { "sadd8",              &MacroExpander::handle_sadd8              },
    { "sadd16",             &MacroExpander::handle_sadd16             },
    { "sub8",               &MacroExpander::handle_sub8               },
    { "sub16",              &MacroExpander::handle_sub16              },
    { "ssub8",              &MacroExpander::handle_ssub8              },
    { "ssub16",             &MacroExpander::handle_ssub16             },
    { "neg8",               &MacroExpander::handle_neg8               },
    { "neg16",              &MacroExpander::handle_neg16              },
    { "sign8",              &MacroExpander::handle_sign8              },
    { "sign16",             &MacroExpander::handle_sign16             },
    { "abs8",               &MacroExpander::handle_abs8               },
    { "abs16",              &MacroExpander::handle_abs16              },
    { "mul8",               &MacroExpander::handle_mul8               },
    { "mul16",              &MacroExpander::handle_mul16              },
    { "smul8",              &MacroExpander::handle_smul8              },
    { "smul16",             &MacroExpander::handle_smul16             },
    { "div8",               &MacroExpander::handle_div8               },
    { "div16",              &MacroExpander::handle_div16              },
    { "sdiv8",              &MacroExpander::handle_sdiv8              },
    { "sdiv16",             &MacroExpander::handle_sdiv16             },
    { "mod8",               &MacroExpander::handle_mod8               },
    { "mod16",              &MacroExpander::handle_mod16              },
    { "smod8",              &MacroExpander::handle_smod8              },
    { "smod16",             &MacroExpander::handle_smod16             },
    { "eq8",                &MacroExpander::handle_eq8                },
    { "eq16",               &MacroExpander::handle_eq16               },
    { "seq8",               &MacroExpander::handle_seq8               },
    { "seq16",              &MacroExpander::handle_seq16              },
    { "ne8",                &MacroExpander::handle_ne8                },
    { "ne16",               &MacroExpander::handle_ne16               },
    { "sne8",               &MacroExpander::handle_sne8               },
    { "sne16",              &MacroExpander::handle_sne16              },
    { "lt8",                &MacroExpander::handle_lt8                },
    { "lt16",               &MacroExpander::handle_lt16               },
    { "slt8",               &MacroExpander::handle_slt8               },
    { "slt16",              &MacroExpander::handle_slt16              },
    { "gt8",                &MacroExpander::handle_gt8                },
    { "gt16",               &MacroExpander::handle_gt16               },
    { "sgt8",               &MacroExpander::handle_sgt8               },
    { "sgt16",              &MacroExpander::handle_sgt16              },
    { "le8",                &MacroExpander::handle_le8                },
    { "le16",               &MacroExpander::handle_le16               },
    { "sle8",               &MacroExpander::handle_sle8               },
    { "sle16",              &MacroExpander::handle_sle16              },
    { "ge8",                &MacroExpander::handle_ge8                },
    { "ge16",               &MacroExpander::handle_ge16               },
    { "sge8",               &MacroExpander::handle_sge8               },
    { "sge16",              &MacroExpander::handle_sge16              },
    { "shr8",               &MacroExpander::handle_shr8               },
    { "shr16",              &MacroExpander::handle_shr16              },
    { "shl8",               &MacroExpander::handle_shl8               },
    { "shl16",              &MacroExpander::handle_shl16              },
    { "if",                 &MacroExpander::handle_if                 },
    { "else",               &MacroExpander::handle_else               },
    { "endif",              &MacroExpander::handle_endif              },
    { "while",              &MacroExpander::handle_while              },
    { "endwhile",           &MacroExpander::handle_endwhile           },
    { "repeat",             &MacroExpander::handle_repeat             },
    { "endrepeat",          &MacroExpander::handle_endrepeat          },
    { "push8",              &MacroExpander::handle_push8              },
    { "push16",             &MacroExpander::handle_push16             },
    { "push8i",             &MacroExpander::handle_push8i             },
    { "push16i",            &MacroExpander::handle_push16i            },
    { "pop8",               &MacroExpander::handle_pop8               },
    { "pop16",              &MacroExpander::handle_pop16              },
    { "alloc_global16",     &MacroExpander::handle_alloc_global16     },
    { "free_global16",      &MacroExpander::handle_free_global16      },
    { "alloc_temp16",       &MacroExpander::handle_alloc_temp16       },
    { "free_temp16",        &MacroExpander::handle_free_temp16        },
    { "enter_frame16",      &MacroExpander::handle_enter_frame16      },
    { "leave_frame16",      &MacroExpander::handle_leave_frame16      },
    { "frame_alloc_temp16", &MacroExpander::handle_frame_alloc_temp16 },
    { "print_char",         &MacroExpander::handle_print_char         },
    { "print_char8",        &MacroExpander::handle_print_char8        },
    { "print_string",       &MacroExpander::handle_print_string       },
    { "print_newline",      &MacroExpander::handle_print_newline      },
    { "print_cell8",        &MacroExpander::handle_print_cell8        },
    { "print_cell16",       &MacroExpander::handle_print_cell16       },
    { "print_cell8s",       &MacroExpander::handle_print_cell8s       },
    { "print_cell16s",      &MacroExpander::handle_print_cell16s      },
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

MacroExpander::MacroExpander(MacroTable& table, Parser* parser)
    : table_(table), parser_(parser) {
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

bool MacroExpander::handle_alloc_cell8(Parser& parser, const Token& tok) {
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
    std::string mock_filename = "(alloc_cell8)";
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

bool MacroExpander::handle_free_cell8(Parser& parser, const Token& tok) {
    std::string macro_name;
    if (!parse_ident_arg(parser, tok, macro_name)) {
        return true; // error already reported
    }

    const Macro* m = g_macro_table.lookup(macro_name);
    if (!m) {
        g_error_reporter.report_error(tok.loc, "free_cell8: macro '" + macro_name + "' not defined");
        return true;
    }
    if (!m->params.empty() || m->body.size() != 1 || m->body[0].type != TokenType::Integer) {
        g_error_reporter.report_error(tok.loc, "free_cell8: '" + macro_name + "' is not an alloc_cell8 result");
        return true;
    }

    int addr = m->body[0].int_value;
    parser.output().free_cells(addr);
    g_macro_table.undef(macro_name);

    TokenScanner scanner;
    std::string mock_filename = "(free_cell8)";
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
        g_error_reporter.report_error(tok.loc, "free_cell16: macro '" + macro_name + "' not defined");
        return true;
    }
    if (!m->params.empty() || m->body.size() != 1 || m->body[0].type != TokenType::Integer) {
        g_error_reporter.report_error(tok.loc, "free_cell16: '" + macro_name + "' is not an alloc_cell16 result");
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

bool MacroExpander::handle_clear8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int value = vals[0];

    TokenScanner scanner;
    std::string mock_filename = "(clear8)";
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

bool MacroExpander::handle_set8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];
    b &= 0xFF;

    TokenScanner scanner;
    std::string mock_filename = "(set8)";
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
    std::string mock_filename = "(set8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(a) + " [-] +" + std::to_string(b_low) +
            "  >" + std::to_string(a + 1) + " [-] +" + std::to_string(b_high) +
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_move8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(move8)";
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
    std::string mock_filename = "(move16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "move8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "move8(" + std::to_string(a + 1) + ", " + std::to_string(b + 1) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_copy8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "a", "b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_name = make_temp_name("t_name");

    TokenScanner scanner;
    std::string mock_filename = "(copy8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + t_name + ")"
            " >" + std::to_string(b) + " [-]"
            " >" + std::to_string(a) +
            " [ - >" + std::to_string(b) +
            " + >" + t_name +
            " + >" + std::to_string(a) + " ]"
            " >" + t_name +
            " [ - >" + std::to_string(a) +
            " + >" + t_name + " ]"
            " free_cell8(" + t_name + ") }",
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
    std::string mock_filename = "(copy16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "copy8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "copy8(" + std::to_string(a + 1) + ", " + std::to_string(b + 1) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_not8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int X = vals[0];

    std::string T = make_temp_name("T");
    std::string F = make_temp_name("F");

    TokenScanner scanner;
    std::string mock_filename = "(not8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate T and F
            "{ alloc_cell8(" + T + ") "
            "  alloc_cell8(" + F + ") "
            // Move X to T destoying X
            "  move8(" + std::to_string(X) + ", " + T + ") "
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
            "  free_cell8(" + T + ") "
            "  free_cell8(" + F + ") "
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

    std::string T1 = make_temp_name("T1");
    std::string T2 = make_temp_name("T2");

    TokenScanner scanner;
    std::string mock_filename = "(not16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T1 + ") "
            "  alloc_cell8(" + T2 + ") "
            // T1 = (a_lo == 0)
            "  copy8(" + std::to_string(a) + ", " + T1 + ") "
            "  not8(" + T1 + ") "
            // T2 = (a_hi == 0)
            "  copy8(" + std::to_string(a + 1) + ", " + T2 + ") "
            "  not8(" + T2 + ") "
            // T1 = T1 AND T2
            "  and8(" + T1 + ", " + T2 + ") "
            // Write result back into a (16-bit)
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell8(" + T1 + ") "
            "  free_cell8(" + T2 + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_and8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_a = make_temp_name("t_a");
    std::string t_b = make_temp_name("t_b");
    std::string t_r = make_temp_name("t_r");

    TokenScanner scanner;
    std::string mock_filename = "(and8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + t_a + ")"
            "  alloc_cell8(" + t_b + ")"
            "  alloc_cell8(" + t_r + ")"
            // booleanize a to t_a, destroys a
            "  move8(" + std::to_string(a) + ", " + t_a + ") "
            "  not8(" + t_a + ") "
            "  not8(" + t_a + ") "
            // booleanize b to t_b, keeps b intact
            "  copy8(" + std::to_string(b) + ", " + t_b + ") "
            "  not8(" + t_b + ") "
            "  not8(" + t_b + ") "
            // if t_a==1 move t_b to t_r, else t_r = 0
            "  >" + t_a + " [ - move8(" + t_b + ", " + t_r + ") ] "
            // move t_r to a
            "  move8(" + t_r + ", " + std::to_string(a) + ") "
            // free temps
            "  free_cell8(" + t_a + ") "
            "  free_cell8(" + t_b + ") "
            "  free_cell8(" + t_r + ") "
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

    std::string T1 = make_temp_name("T1");
    std::string T2 = make_temp_name("T2");

    TokenScanner scanner;
    std::string mock_filename = "(and16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T1 + ") "
            "  alloc_cell8(" + T2 + ") "
            // T1 = (a_lo != 0) OR (a_hi != 0)
            "  copy8(" + std::to_string(a) + ", " + T1 + ") "
            "  or8(" + T1 + ", " + std::to_string(a + 1) + ") "
            // T2 = (b_lo != 0) OR (b_hi != 0)
            "  copy8(" + std::to_string(b) + ", " + T2 + ") "
            "  or8(" + T2 + ", " + std::to_string(b + 1) + ") "
            // T1 = T1 AND ((b_lo != 0) OR (b_hi != 0))
            "  and8(" + T1 + ", " + T2 + ") "
            // if T1 != 0 set a=1 else a=0
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            // free temps
            "  free_cell8(" + T1 + ") "
            "  free_cell8(" + T2 + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_or8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_a = make_temp_name("t_a");
    std::string t_b = make_temp_name("t_b");
    std::string t_r = make_temp_name("t_r");

    TokenScanner scanner;
    std::string mock_filename = "(or8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + t_a + ")"
            "  alloc_cell8(" + t_b + ")"
            "  alloc_cell8(" + t_r + ")"
            // booleanize a to t_a, destroys a
            "  move8(" + std::to_string(a) + ", " + t_a + ") "
            "  not8(" + t_a + ") "
            "  not8(" + t_a + ") "
            // booleanize b to t_b, keeps b intact
            "  copy8(" + std::to_string(b) + ", " + t_b + ") "
            "  not8(" + t_b + ") "
            "  not8(" + t_b + ") "
            // add t_a and t_b to t_r (result = 0, 1 or 2)
            "  >" + t_a + " [ - >" + t_r + " + >" + t_a + " ] "
            "  >" + t_b + " [ - >" + t_r + " + >" + t_b + " ] "
            // booleanize t_r to 0 or 1
            "  not8(" + t_r + ") "
            "  not8(" + t_r + ") "
            // move t_r to a
            "  move8(" + t_r + ", " + std::to_string(a) + ") "
            // free temps
            "  free_cell8(" + t_a + ") "
            "  free_cell8(" + t_b + ") "
            "  free_cell8(" + t_r + ") "
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

    std::string T = make_temp_name("T");

    TokenScanner scanner;
    std::string mock_filename = "(or16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T + ") "
            // T = a_lo OR a_hi
            "  copy8(" + std::to_string(a) + ", " + T + ") "
            "  or8(" + T + ", " + std::to_string(a + 1) + ") "
            // T = T OR b_lo
            "  or8(" + T + ", " + std::to_string(b) + ") "
            // T = T OR b_hi
            "  or8(" + T + ", " + std::to_string(b + 1) + ") "
            //  ---- write result back into a ----
            "  if(" + T + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell8(" + T + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_xor8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T1 = make_temp_name("T1");
    std::string T2 = make_temp_name("T2");

    TokenScanner scanner;
    std::string mock_filename = "(xor8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T1 + ")"
            "  alloc_cell8(" + T2 + ")"
            // Compute A_OR_B into T1
            "  copy8(" + std::to_string(a) + ", " + T1 + ") "
            "  or8(" + T1 + ", " + std::to_string(b) + ") "
            // Compute A_AND_B into T2
            "  copy8(" + std::to_string(a) + ", " + T2 + ") "
            "  and8(" + T2 + ", " + std::to_string(b) + ") "
            // NOT(T2)
            "  not8(" + T2 + ") "
            // XOR = (A OR B) AND NOT(A AND B)
            "  copy8(" + T1 + ", " + std::to_string(a) + ") "
            "  and8(" + std::to_string(a) + ", " + T2 + ") "
            // free temps
            "  free_cell8(" + T1 + ") "
            "  free_cell8(" + T2 + ") "
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

    std::string T1 = make_temp_name("T1");
    std::string T2 = make_temp_name("T2");

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

bool MacroExpander::handle_add8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T = make_temp_name("T");

    TokenScanner scanner;
    std::string mock_filename = "(add8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate T variable
            "{ alloc_cell8(" + T + ") "
            // copy b to T
            "  copy8(" + std::to_string(b) + ", " + T + ") "
            // add T to a
            "  >" + T +
            " [ - >" + std::to_string(a) + " + >" + T + " ] "
            // free T
            "  free_cell8(" + T + ") "
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

    std::string t_old = make_temp_name("t_old");
    std::string t_carry = make_temp_name("t_carry");

    TokenScanner scanner;
    std::string mock_filename = "(add16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + t_old + ") "
            "  alloc_cell8(" + t_carry + ") "
            // ---- Step 1: save old low byte ----
            "  copy8(" + std::to_string(a) + ", " + t_old + ") "
            // ---- Step 2: add low bytes ----
            "  add8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // ---- Step 3: detect carry ----
            // T_carry = new a_lo
            "  copy8(" + std::to_string(a) + ", " + t_carry + ") "
            // carry = (a_lo < old)
            "  lt8(" + t_carry + ", " + t_old + ") "
            // ---- Step 4: add high bytes ----
            "  add8(" + std::to_string(a + 1) + ", " + std::to_string(b + 1) + ") "
            // ---- Step 4: add carry into high byte ----
            "  add8(" + std::to_string(a + 1) + ", " + t_carry + ") "
            "  free_cell8(" + t_old + ") "
            "  free_cell8(" + t_carry + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_sadd8(Parser& parser, const Token& tok) {
    return handle_add8(parser, tok);
}

bool MacroExpander::handle_sadd16(Parser& parser, const Token& tok) {
    return handle_add16(parser, tok);
}

bool MacroExpander::handle_sub8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T = make_temp_name("T");

    TokenScanner scanner;
    std::string mock_filename = "(sub8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate T variable
            "{ alloc_cell8(" + T + ") "
            // copy b to T
            "  copy8(" + std::to_string(b) + ", " + T + ") "
            // sub T from a
            "  >" + T +
            " [ - >" + std::to_string(a) + " - >" + T + " ] "
            // free T
            "  free_cell8(" + T + ") "
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

    std::string t_old = make_temp_name("t_old");
    std::string t_borrow = make_temp_name("t_borrow");

    TokenScanner scanner;
    std::string mock_filename = "(sub16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + t_old + ") "
            "  alloc_cell8(" + t_borrow + ") "
            // ---- Step 1: save old low byte ----
            "  copy8(" + std::to_string(a) + ", " + t_old + ") "
            // ---- Step 2: sub low bytes ----
            "  sub8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // ---- Step 3: detect borrow ----
            // T_borrow = new a_lo
            "  copy8(" + std::to_string(a) + ", " + t_borrow + ") "
            // borrow = (a_lo > old)
            "  gt8(" + t_borrow + ", " + t_old + ") "
            // ---- Step 4: sub high bytes ----
            "  sub8(" + std::to_string(a + 1) + ", " + std::to_string(b + 1) + ") "
            // ---- Step 4: sub borrow into high byte ----
            "  sub8(" + std::to_string(a + 1) + ", " + t_borrow + ") "
            "  free_cell8(" + t_old + ") "
            "  free_cell8(" + t_borrow + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_ssub8(Parser& parser, const Token& tok) {
    return handle_sub8(parser, tok);
}

bool MacroExpander::handle_ssub16(Parser& parser, const Token& tok) {
    return handle_sub16(parser, tok);
}

bool MacroExpander::handle_neg8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int a = vals[0];

    std::string T_zero = make_temp_name("T_zero");

    TokenScanner scanner;
    std::string mock_filename = "(neg8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate T and F
            "{ alloc_cell8(" + T_zero + ") "
            "  sub8(" + T_zero + ", " + std::to_string(a) + ") "
            "  move8(" + T_zero + ", " + std::to_string(a) + ") "
            "  free_cell8(" + T_zero + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_neg16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int a = vals[0];

    std::string T_zero = make_temp_name("T_zero");

    TokenScanner scanner;
    std::string mock_filename = "(neg16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate T and F
            "{ alloc_cell16(" + T_zero + ") "
            "  sub16(" + T_zero + ", " + std::to_string(a) + ") "
            "  move16(" + T_zero + ", " + std::to_string(a) + ") "
            "  free_cell16(" + T_zero + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_mul8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_res = make_temp_name("T_res");
    std::string T_b = make_temp_name("T_b");
    std::string T_tmp = make_temp_name("T_tmp");
    std::string T_one = make_temp_name("T_one");
    std::string T_two = make_temp_name("T_two");

    TokenScanner scanner;
    std::string mock_filename = "(mul8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T_res + ") "
            "  alloc_cell8(" + T_b + ") "
            "  alloc_cell8(" + T_tmp + ") "
            "  alloc_cell8(" + T_one + ") >" + T_one + " + "
            "  alloc_cell8(" + T_two + ") >" + T_two + " ++ "
            "  copy8(" + std::to_string(b) + ", " + T_b + ") "
            "  while(" + T_b + ") "
            //   if (b is odd)
            "    copy8(" + T_b + ", " + T_tmp + ") "
            "    mod8(" + T_tmp + ", " + T_two + ") "
            "    if(" + T_tmp + ") "
            //     add a to result
            "      add8(" + T_res + ", " + std::to_string(a) + ") "
            "    endif "
            //   b = b // 2
            "    shr8(" + T_b + ", " + T_one + ") "
            //   a = a * 2
            "    shl8(" + std::to_string(a) + ", " + T_one + ") "
            "  endwhile "
            // move result back to a
            "  move8(" + T_res + ", " + std::to_string(a) + ") "
            // free T
            "  free_cell8(" + T_res + ") "
            "  free_cell8(" + T_b + ") "
            "  free_cell8(" + T_tmp + ") "
            "  free_cell8(" + T_one + ") "
            "  free_cell8(" + T_two + ") "
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

    std::string T_acc = make_temp_name("T_acc");
    std::string T_mul = make_temp_name("T_mul");
    std::string T_mcand = make_temp_name("T_mcand");
    std::string T_tmp = make_temp_name("T_tmp");
    std::string T_one = make_temp_name("T_one");
    std::string T_two = make_temp_name("T_two");

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

bool MacroExpander::handle_smul8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_sign_a = make_temp_name("T_sign_a");
    std::string T_sign_b = make_temp_name("T_sign_b");
    std::string T_final_sign = make_temp_name("T_final_sign");
    std::string T_b_copy = make_temp_name("T_b_copy");

    TokenScanner scanner;
    std::string mock_filename = "(smul8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T_sign_a + ") "
            "  alloc_cell8(" + T_sign_b + ") "
            "  alloc_cell8(" + T_final_sign + ") "
            "  alloc_cell8(" + T_b_copy + ") "
            // extract sign(a)
            "  copy8(" + std::to_string(a) + ", " + T_sign_a + ") "
            "  sign8(" + T_sign_a + ") "
            // extract sign(b)
            "  copy8(" + std::to_string(b) + ", " + T_sign_b + ") "
            "  sign8(" + T_sign_b + ") "
            // final_sign = sign(a) XOR sign(b)
            "  copy8(" + T_sign_a + ", " + T_final_sign + ") "
            "  xor8(" + T_final_sign + ", " + T_sign_b + ") "
            // abs(a) -> a
            "  abs8(" + std::to_string(a) + ") "
            // copy b to T_b_copy and abs(T_b_copy) -> T_b_copy
            "  copy8(" + std::to_string(b) + ", " + T_b_copy + ") "
            "  abs8(" + T_b_copy + ") "
            // unsigned multiply abs(a) and abs(b), store result in a
            "  mul8(" + std::to_string(a) + ", " + T_b_copy + ") "
            // if final_sign is negative, negate a
            "  if(" + T_final_sign + ") "
            "    neg8(" + std::to_string(a) + ") "
            "  endif "
            // free temps
            "  free_cell8(" + T_sign_a + ") "
            "  free_cell8(" + T_sign_b + ") "
            "  free_cell8(" + T_final_sign + ") "
            "  free_cell8(" + T_b_copy + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_smul16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_sign_a = make_temp_name("T_sign_a");
    std::string T_sign_b = make_temp_name("T_sign_b");
    std::string T_final_sign = make_temp_name("T_final_sign");
    std::string T_b_copy = make_temp_name("T_b_copy");

    TokenScanner scanner;
    std::string mock_filename = "(smul16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell16(" + T_sign_a + ") "
            "  alloc_cell16(" + T_sign_b + ") "
            "  alloc_cell16(" + T_final_sign + ") "
            "  alloc_cell16(" + T_b_copy + ") "
            // extract sign(a)
            "  copy16(" + std::to_string(a) + ", " + T_sign_a + ") "
            "  sign16(" + T_sign_a + ") "
            // extract sign(b)
            "  copy16(" + std::to_string(b) + ", " + T_sign_b + ") "
            "  sign16(" + T_sign_b + ") "
            // final_sign = sign(a) XOR sign(b)
            "  copy16(" + T_sign_a + ", " + T_final_sign + ") "
            "  xor16(" + T_final_sign + ", " + T_sign_b + ") "
            // abs(a) -> a
            "  abs16(" + std::to_string(a) + ") "
            // copy b to T_b_copy and abs(T_b_copy) -> T_b_copy
            "  copy16(" + std::to_string(b) + ", " + T_b_copy + ") "
            "  abs16(" + T_b_copy + ") "
            // unsigned multiply abs(a) and abs(b), store result in a
            "  mul16(" + std::to_string(a) + ", " + T_b_copy + ") "
            // if final_sign is negative, negate a
            "  if(" + T_final_sign + ") "
            "    neg16(" + std::to_string(a) + ") "
            "  endif "
            // free temps
            "  free_cell16(" + T_sign_a + ") "
            "  free_cell16(" + T_sign_b + ") "
            "  free_cell16(" + T_final_sign + ") "
            "  free_cell16(" + T_b_copy + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_div8(Parser& parser, const Token& tok) {
    return handle_div8_mod8(parser, tok, /*return_remainder=*/false);
}

bool MacroExpander::handle_div16(Parser& parser, const Token& tok) {
    return handle_div16_mod16(parser, tok, /*return_remainder=*/false);
}

bool MacroExpander::handle_sdiv8(Parser& parser, const Token& tok) {
    return handle_sdiv8_smod8(parser, tok, /*return_remainder=*/false);
}

bool MacroExpander::handle_sdiv16(Parser& parser, const Token& tok) {
    return handle_sdiv16_smod16(parser, tok, /*return_remainder=*/false);
}

bool MacroExpander::handle_mod8(Parser& parser, const Token& tok) {
    return handle_div8_mod8(parser, tok, /*return_remainder=*/true);
}

bool MacroExpander::handle_mod16(Parser& parser, const Token& tok) {
    return handle_div16_mod16(parser, tok, /*return_remainder=*/true);
}

bool MacroExpander::handle_smod8(Parser& parser, const Token& tok) {
    return handle_sdiv8_smod8(parser, tok, /*return_remainder=*/true);
}

bool MacroExpander::handle_smod16(Parser& parser, const Token& tok) {
    return handle_sdiv16_smod16(parser, tok, /*return_remainder=*/true);
}

bool MacroExpander::handle_div8_mod8(Parser& parser, const Token& tok,
                                     bool return_remainder) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_quot = make_temp_name("T_quot");
    std::string T_rem = make_temp_name("T_rem");
    std::string T_bit = make_temp_name("T_bit");
    std::string T_tmp = make_temp_name("T_tmp");
    std::string T_one = make_temp_name("T_one");
    std::string T_seven = make_temp_name("T_seven");
    std::string T_eight = make_temp_name("T_eight");

    const std::string move_target = return_remainder ? T_rem : T_quot;
    const std::string mock_filename = return_remainder ? "(mod8)" : "(div8)";

    TokenScanner scanner;
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T_quot + ") "
            "  alloc_cell8(" + T_rem + ") "
            "  alloc_cell8(" + T_bit + ") "
            "  alloc_cell8(" + T_tmp + ") "
            "  alloc_cell8(" + T_one + ") >" + T_one + " + "
            "  alloc_cell8(" + T_seven + ") >" + T_seven + " +7 "
            "  alloc_cell8(" + T_eight + ") >" + T_eight + " +8 "
            "  if(" + std::to_string(b) + ") "
            "    repeat(" + T_eight + ") "
            "      copy8(" + std::to_string(a) + ", " + T_bit + ") "
            "      shr8(" + T_bit + ", " + T_seven + ") "
            "      shl8(" + std::to_string(a) + ", " + T_one + ") "
            "      shl8(" + T_rem + ", " + T_one + ") "
            "      add8(" + T_rem + ", " + T_bit + ") "
            "      copy8(" + T_rem + ", " + T_tmp + ") "
            "      ge8(" + T_tmp + ", " + std::to_string(b) + ") "
            "      if(" + T_tmp + ") "
            "        sub8(" + T_rem + ", " + std::to_string(b) + ") "
            "        shl8(" + T_quot + ", " + T_one + ") "
            "        add8(" + T_quot + ", " + T_one + ") "
            "      else "
            "        shl8(" + T_quot + ", " + T_one + ") "
            "      endif "
            "    endrepeat "
            "    move8(" + move_target + ", " + std::to_string(a) + ") "
            "  endif "
            "  free_cell8(" + T_quot + ") "
            "  free_cell8(" + T_rem + ") "
            "  free_cell8(" + T_bit + ") "
            "  free_cell8(" + T_tmp + ") "
            "  free_cell8(" + T_one + ") "
            "  free_cell8(" + T_seven + ") "
            "  free_cell8(" + T_eight + ") "
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

    std::string T_work = make_temp_name("T_work");
    std::string T_quot = make_temp_name("T_quot");
    std::string T_scale = make_temp_name("T_scale");
    std::string T_bit = make_temp_name("T_bit");
    std::string T_tmp = make_temp_name("T_tmp");
    std::string T_cond = make_temp_name("T_cond");
    std::string T_guard = make_temp_name("T_guard");
    std::string T_one = make_temp_name("T_one");

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
            "  alloc_cell16(" + T_guard + ") "
            "  alloc_cell16(" + T_one + ") set16(" + T_one + ", 1) "
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
            //     add a guard for overflow guard = (tmp > scale)
            "      copy16(" + T_tmp + ", " + T_guard + ") "
            "      gt16(" + T_guard + ", " + T_scale + ") "
            "      and16(" + T_cond + ", " + T_guard + ") "
            "      while (" + T_cond + ") "
            "        shl16(" + T_scale + ", " + T_one + ") "
            "        shl16(" + T_bit + ", " + T_one + ") "
            //       recompute loop condition
            "        copy16(" + T_scale + ", " + T_tmp + ") "
            "        shl16(" + T_tmp + ", " + T_one + ") "
            "        copy16(" + T_work + ", " + T_cond + ") "
            "        ge16(" + T_cond + ", " + T_tmp + ") "
            //       add a guard for overflow guard = (tmp > scale)
            "        copy16(" + T_tmp + ", " + T_guard + ") "
            "        gt16(" + T_guard + ", " + T_scale + ") "
            "        and16(" + T_cond + ", " + T_guard + ") "
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
            "  free_cell16(" + T_guard + ") "
            "  free_cell16(" + T_one + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_sdiv8_smod8(Parser& parser, const Token& tok,
                                       bool return_remainder) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_final_sign = make_temp_name("t_final_sign");
    std::string t_b_abs = make_temp_name("t_b_abs");

    const std::string mock_filename = return_remainder ? "(smod8)" : "(sdiv8)";
    const std::string final_sign =
        return_remainder ?
        // final_sign = sa
        "  copy8(" + t_sa + ", " + t_final_sign + ") "
        :
        // final_sign = sa XOR sb
        "  copy8(" + t_sa + ", " + t_final_sign + ") "
        "  xor8(" + t_final_sign + ", " + t_sb + ") "
        ;
    const std::string operation = return_remainder ? "mod8" : "div8";

    TokenScanner scanner;
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + t_sa + ") "
            "  alloc_cell8(" + t_sb + ") "
            "  alloc_cell8(" + t_final_sign + ") "
            "  alloc_cell8(" + t_b_abs + ") "
            // sa = sign(a)
            "  copy8(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign8(" + t_sa + ") "
            // sb = sign(b)
            "  copy8(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign8(" + t_sb + ") " +
            // compute final_sign and move it into t_final_sign
            final_sign +
            // abs(a), abs(b)
            "  abs8(" + std::to_string(a) + ") "
            "  copy8(" + std::to_string(b) + ", " + t_b_abs + ") "
            "  abs8(" + t_b_abs + ") "
            // do div/mod operation on abs values, store result in a
            "  " + operation + "(" + std::to_string(a) + ", " + t_b_abs + ") "
            // apply sign to result if necessary
            "  if(" + t_final_sign + ") "
            "    neg8(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell8(" + t_sa + ") "
            "  free_cell8(" + t_sb + ") "
            "  free_cell8(" + t_final_sign + ") "
            "  free_cell8(" + t_b_abs + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_sdiv16_smod16(Parser& parser, const Token& tok,
        bool return_remainder) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_final_sign = make_temp_name("t_final_sign");
    std::string t_b_abs = make_temp_name("t_b_abs");

    const std::string mock_filename = return_remainder ? "(smod16)" : "(sdiv16)";
    const std::string final_sign =
        return_remainder ?
        // final_sign = sa
        "  copy16(" + t_sa + ", " + t_final_sign + ") "
        :
        // final_sign = sa XOR sb
        "  copy16(" + t_sa + ", " + t_final_sign + ") "
        "  xor16(" + t_final_sign + ", " + t_sb + ") "
        ;
    const std::string operation = return_remainder ? "mod16" : "div16";

    TokenScanner scanner;
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell16(" + t_sa + ") "
            "  alloc_cell16(" + t_sb + ") "
            "  alloc_cell16(" + t_final_sign + ") "
            "  alloc_cell16(" + t_b_abs + ") "
            // sa = sign(a)
            "  copy16(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign16(" + t_sa + ") "
            // sb = sign(b)
            "  copy16(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign16(" + t_sb + ") " +
            // compute final_sign and move it into t_final_sign
            final_sign +
            // abs(a), abs(b)
            "  abs16(" + std::to_string(a) + ") "
            "  copy16(" + std::to_string(b) + ", " + t_b_abs + ") "
            "  abs16(" + t_b_abs + ") "
            // do div/mod operation on abs values, store result in a
            "  " + operation + "(" + std::to_string(a) + ", " + t_b_abs + ") "
            // apply sign to result if necessary
            "  if(" + t_final_sign + ") "
            "    neg16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell16(" + t_sa + ") "
            "  free_cell16(" + t_sb + ") "
            "  free_cell16(" + t_final_sign + ") "
            "  free_cell16(" + t_b_abs + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_sign8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int x = vals[0];

    std::string T_128 = make_temp_name("T_128");

    TokenScanner scanner;
    std::string mock_filename = "(sign8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate temps
            "{ alloc_cell8(" + T_128 + ") "
            // compute sign = (x >= 128)
            "  set8(" + T_128 + ", 128) "
            "  ge8(" + std::to_string(x) + ", " + T_128 + ") "
            "  free_cell8(" + T_128 + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_sign16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int x = vals[0];

    std::string T_32768 = make_temp_name("T_32768");

    TokenScanner scanner;
    std::string mock_filename = "(sign16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate temps
            "{ alloc_cell16(" + T_32768 + ") "
            // compute sign = (x >= 32768)
            "  set16(" + T_32768 + ", 32768) "
            "  ge16(" + std::to_string(x) + ", " + T_32768 + ") "
            "  free_cell16(" + T_32768 + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_abs8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int x = vals[0];

    std::string T_cond = make_temp_name("T_cond");

    TokenScanner scanner;
    std::string mock_filename = "(abs8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate temps
            "{ alloc_cell8(" + T_cond + ") "
            // compute sign
            "  copy8(" + std::to_string(x) + ", " + T_cond + ") "
            "  sign8(" + T_cond + ") "
            // if negative, negate x
            "  if(" + T_cond + ") "
            "    neg8(" + std::to_string(x) + ") "
            "  endif "
            "  free_cell8(" + T_cond + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_abs16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr" }, vals)) {
        return true;
    }
    int x = vals[0];

    std::string T_cond = make_temp_name("T_cond");

    TokenScanner scanner;
    std::string mock_filename = "(abs16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate temps
            "{ alloc_cell16(" + T_cond + ") "
            // compute sign
            "  copy16(" + std::to_string(x) + ", " + T_cond + ") "
            "  sign16(" + T_cond + ") "
            // if negative, negate x
            "  if(" + T_cond + ") "
            "    neg16(" + std::to_string(x) + ") "
            "  endif "
            "  free_cell16(" + T_cond + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_eq8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(eq8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // a -= b
            "sub8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // not(a): 0 (equal) -> 1, non-zero (not equal) -> 0
            "not8(" + std::to_string(a) + ") ",
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

    std::string T1 = make_temp_name("T1");
    std::string T2 = make_temp_name("T2");

    TokenScanner scanner;
    std::string mock_filename = "(eq16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T1 + ") "
            "  alloc_cell8(" + T2 + ") "
            // T1 = (a_lo == b_lo)
            "  copy8(" + std::to_string(a) + ", " + T1 + ") "
            "  eq8(" + T1 + ", " + std::to_string(b) + ") "
            // T2 = (a_hi == b_hi)
            "  copy8(" + std::to_string(a + 1) + ", " + T2 + ") "
            "  eq8(" + T2 + ", " + std::to_string(b + 1) + ") "
            // T1 = T1 AND T2
            "  and8(" + T1 + ", " + T2 + ") "
            // copy the result back into a (16-bit)
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell8(" + T1 + ") "
            "  free_cell8(" + T2 + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_seq8(Parser& parser, const Token& tok) {
    return handle_eq8(parser, tok);
}

bool MacroExpander::handle_seq16(Parser& parser, const Token& tok) {
    return handle_eq16(parser, tok);
}

bool MacroExpander::handle_ne8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(ne8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // a == b
            "eq8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            // not(a)
            "not8(" + std::to_string(a) + ") ",
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

bool MacroExpander::handle_sne8(Parser& parser, const Token& tok) {
    return handle_ne8(parser, tok);
}

bool MacroExpander::handle_sne16(Parser& parser, const Token& tok) {
    return handle_ne16(parser, tok);
}

bool MacroExpander::handle_lt8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_a = make_temp_name("t_a");
    std::string t_b = make_temp_name("t_b");
    std::string t_a_and_b = make_temp_name("t_a_and_b");
    std::string temp_lt = make_temp_name("temp_lt");

    TokenScanner scanner;
    std::string mock_filename = "(lt8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell8(" + t_a + ") "
            "  alloc_cell8(" + t_b + ") "
            "  alloc_cell8(" + t_a_and_b + ") "
            "  alloc_cell8(" + temp_lt + ") "
            // copy a to t_a and b to t_b
            "  copy8(" + std::to_string(a) + ", " + t_a + ") "
            "  copy8(" + std::to_string(b) + ", " + t_b + ") "
            // while t_a > 0 and t_b > 0, decrement both
            "  copy8(" + t_a + ", " + t_a_and_b + ") "
            "  and8(" + t_a_and_b + ", " + t_b + ") "
            "  while(" + t_a_and_b + ") "
            "    >" + t_a + " - "
            "    >" + t_b + " - "
            "    copy8(" + t_a + ", " + t_a_and_b + ") "
            "    and8(" + t_a_and_b + ", " + t_b + ") "
            "  endwhile "
            // if t_a == 0 and t_b > 0, then a < b
            "  clear8(" + std::to_string(a) + ") "
            "  copy8(" + t_a + ", " + temp_lt + ") "
            "  not8(" + temp_lt + ") "
            "  and8(" + temp_lt + ", " + t_b + ") "
            "  if(" + temp_lt + ") "
            "    >" + std::to_string(a) + " + "
            "  endif "
            // free temp variables
            "  free_cell8(" + t_a + ") "
            "  free_cell8(" + t_b + ") "
            "  free_cell8(" + t_a_and_b + ") "
            "  free_cell8(" + temp_lt + ") "
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

    std::string T1 = make_temp_name("T1");
    std::string T2 = make_temp_name("T2");

    TokenScanner scanner;
    std::string mock_filename = "(lt16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T1 + ") "
            "  alloc_cell8(" + T2 + ") "
            // T1 = (a_hi < b_hi)
            "  copy8(" + std::to_string(a + 1) + ", " + T1 + ") "
            "  lt8(" + T1 + ", " + std::to_string(b + 1) + ") "
            // T2 = (a_hi == b_hi)
            "  copy8(" + std::to_string(a + 1) + ", " + T2 + ") "
            "  eq8(" + T2 + ", " + std::to_string(b + 1) + ") "
            // If high bytes equal, refine result with low-byte comparison
            "  if(" + T2 + ") "
            // T1 = (a_lo < b_lo)
            "    copy8(" + std::to_string(a) + ", " + T1 + ") "
            "    lt8(" + T1 + ", " + std::to_string(b) + ") "
            "  endif "
            // copy the result back into a (16-bit)
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell8(" + T1 + ") "
            "  free_cell8(" + T2 + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_slt8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_tmp = make_temp_name("t_tmp");

    TokenScanner scanner;
    std::string mock_filename = "(slt8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell8(" + t_sa + ") "
            "  alloc_cell8(" + t_sb + ") "
            "  alloc_cell8(" + t_tmp + ") "
            // extract sign bits
            "  copy8(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign8(" + t_sa + ") "
            "  copy8(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign8(" + t_sb + ") "
            // if signs differ, the negative one is smaller
            "  copy8(" + t_sa + ", " + t_tmp + ") "
            "  xor8(" + t_tmp + ", " + t_sb + ") "
            "  if(" + t_tmp + ") "
            "    copy8(" + t_sa + ", " + std::to_string(a) + ") "
            "  else "
            // if signs are the same, use unsigned comparison
            "    lt8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "  endif "
            // free temp variables
            "  free_cell8(" + t_sa + ") "
            "  free_cell8(" + t_sb + ") "
            "  free_cell8(" + t_tmp + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_slt16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_tmp = make_temp_name("t_tmp");

    TokenScanner scanner;
    std::string mock_filename = "(slt16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell16(" + t_sa + ") "
            "  alloc_cell16(" + t_sb + ") "
            "  alloc_cell16(" + t_tmp + ") "
            // extract sign bits
            "  copy16(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign16(" + t_sa + ") "
            "  copy16(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign16(" + t_sb + ") "
            // if signs differ, the negative one is smaller
            "  copy16(" + t_sa + ", " + t_tmp + ") "
            "  xor16(" + t_tmp + ", " + t_sb + ") "
            "  if(" + t_tmp + ") "
            "    copy16(" + t_sa + ", " + std::to_string(a) + ") "
            "  else "
            // if signs are the same, use unsigned comparison
            "    lt16(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "  endif "
            // free temp variables
            "  free_cell16(" + t_sa + ") "
            "  free_cell16(" + t_sb + ") "
            "  free_cell16(" + t_tmp + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_gt8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_a = make_temp_name("t_a");
    std::string t_b = make_temp_name("t_b");
    std::string t_a_and_b = make_temp_name("t_a_and_b");
    std::string t_gt = make_temp_name("t_gt");

    TokenScanner scanner;
    std::string mock_filename = "(gt8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell8(" + t_a + ") "
            "  alloc_cell8(" + t_b + ") "
            "  alloc_cell8(" + t_a_and_b + ") "
            "  alloc_cell8(" + t_gt + ") "
            // copy a to t_a and b to t_b
            "  copy8(" + std::to_string(a) + ", " + t_a + ") "
            "  copy8(" + std::to_string(b) + ", " + t_b + ") "
            // while t_a > 0 and t_b > 0, decrement both
            "  copy8(" + t_a + ", " + t_a_and_b + ") "
            "  and8(" + t_a_and_b + ", " + t_b + ") "
            "  while(" + t_a_and_b + ") "
            "    >" + t_a + " - "
            "    >" + t_b + " - "
            "    copy8(" + t_a + ", " + t_a_and_b + ") "
            "    and8(" + t_a_and_b + ", " + t_b + ") "
            "  endwhile "
            // if t_b == 0 and t_a > 0, then a > b
            "  clear8(" + std::to_string(a) + ") "
            "  copy8(" + t_b + ", " + t_gt + ") "
            "  not8(" + t_gt + ") "
            "  and8(" + t_gt + ", " + t_a + ") "
            "  if(" + t_gt + ") "
            "    >" + std::to_string(a) + " + "
            "  endif "
            // free temp variables
            "  free_cell8(" + t_a + ") "
            "  free_cell8(" + t_b + ") "
            "  free_cell8(" + t_a_and_b + ") "
            "  free_cell8(" + t_gt + ") "
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

    std::string T1 = make_temp_name("T1");
    std::string T2 = make_temp_name("T2");

    TokenScanner scanner;
    std::string mock_filename = "(gt16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T1 + ") "
            "  alloc_cell8(" + T2 + ") "
            // T1 = (a_hi > b_hi)
            "  copy8(" + std::to_string(a + 1) + ", " + T1 + ") "
            "  gt8(" + T1 + ", " + std::to_string(b + 1) + ") "
            // T2 = (a_hi == b_hi)
            "  copy8(" + std::to_string(a + 1) + ", " + T2 + ") "
            "  eq8(" + T2 + ", " + std::to_string(b + 1) + ") "
            // If high bytes equal, refine result with low-byte comparison
            "  if(" + T2 + ") "
            // T1 = (a_lo > b_lo)
            "    copy8(" + std::to_string(a) + ", " + T1 + ") "
            "    gt8(" + T1 + ", " + std::to_string(b) + ") "
            "  endif "
            // copy the result back into a (16-bit)
            "  if(" + T1 + ") "
            "    set16(" + std::to_string(a) + ", 1) "
            "  else "
            "    clear16(" + std::to_string(a) + ") "
            "  endif "
            "  free_cell8(" + T1 + ") "
            "  free_cell8(" + T2 + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_sgt8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_tmp = make_temp_name("t_tmp");

    TokenScanner scanner;
    std::string mock_filename = "(sgt8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell8(" + t_sa + ") "
            "  alloc_cell8(" + t_sb + ") "
            "  alloc_cell8(" + t_tmp + ") "
            // extract sign bits
            "  copy8(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign8(" + t_sa + ") "
            "  copy8(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign8(" + t_sb + ") "
            // if signs differ, the negative one is smaller
            "  copy8(" + t_sa + ", " + t_tmp + ") "
            "  xor8(" + t_tmp + ", " + t_sb + ") "
            "  if(" + t_tmp + ") "
            "    copy8(" + t_sb + ", " + std::to_string(a) + ") "
            "  else "
            // if signs are the same, use unsigned comparison
            "    gt8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "  endif "
            // free temp variables
            "  free_cell8(" + t_sa + ") "
            "  free_cell8(" + t_sb + ") "
            "  free_cell8(" + t_tmp + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_sgt16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_tmp = make_temp_name("t_tmp");

    TokenScanner scanner;
    std::string mock_filename = "(sgt16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell16(" + t_sa + ") "
            "  alloc_cell16(" + t_sb + ") "
            "  alloc_cell16(" + t_tmp + ") "
            // extract sign bits
            "  copy16(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign16(" + t_sa + ") "
            "  copy16(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign16(" + t_sb + ") "
            // if signs differ, the negative one is smaller
            "  copy16(" + t_sa + ", " + t_tmp + ") "
            "  xor16(" + t_tmp + ", " + t_sb + ") "
            "  if(" + t_tmp + ") "
            "    copy16(" + t_sb + ", " + std::to_string(a) + ") "
            "  else "
            // if signs are the same, use unsigned comparison
            "    gt16(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "  endif "
            // free temp variables
            "  free_cell16(" + t_sa + ") "
            "  free_cell16(" + t_sb + ") "
            "  free_cell16(" + t_tmp + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_le8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(le8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // (a <= b) is !(a > b)
            "gt8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "not8(" + std::to_string(a) + ") ",
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

bool MacroExpander::handle_sle8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_tmp = make_temp_name("t_tmp");

    TokenScanner scanner;
    std::string mock_filename = "(sle8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell8(" + t_sa + ") "
            "  alloc_cell8(" + t_sb + ") "
            "  alloc_cell8(" + t_tmp + ") "
            // extract sign bits
            "  copy8(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign8(" + t_sa + ") "
            "  copy8(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign8(" + t_sb + ") "
            // if signs differ, the negative one is smaller
            "  copy8(" + t_sa + ", " + t_tmp + ") "
            "  xor8(" + t_tmp + ", " + t_sb + ") "
            "  if(" + t_tmp + ") "
            "    copy8(" + t_sa + ", " + std::to_string(a) + ") "
            "  else "
            // if signs are the same, use unsigned comparison
            "    le8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "  endif "
            // free temp variables
            "  free_cell8(" + t_sa + ") "
            "  free_cell8(" + t_sb + ") "
            "  free_cell8(" + t_tmp + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_sle16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_tmp = make_temp_name("t_tmp");

    TokenScanner scanner;
    std::string mock_filename = "(sle16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell16(" + t_sa + ") "
            "  alloc_cell16(" + t_sb + ") "
            "  alloc_cell16(" + t_tmp + ") "
            // extract sign bits
            "  copy16(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign16(" + t_sa + ") "
            "  copy16(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign16(" + t_sb + ") "
            // if signs differ, the negative one is smaller
            "  copy16(" + t_sa + ", " + t_tmp + ") "
            "  xor16(" + t_tmp + ", " + t_sb + ") "
            "  if(" + t_tmp + ") "
            "    copy16(" + t_sa + ", " + std::to_string(a) + ") "
            "  else "
            // if signs are the same, use unsigned comparison
            "    le16(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "  endif "
            // free temp variables
            "  free_cell16(" + t_sa + ") "
            "  free_cell16(" + t_sb + ") "
            "  free_cell16(" + t_tmp + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_ge8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    TokenScanner scanner;
    std::string mock_filename = "(ge8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // (a >= b) is !(a < b)
            "lt8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "not8(" + std::to_string(a) + ") ",
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

bool MacroExpander::handle_sge8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_tmp = make_temp_name("t_tmp");

    TokenScanner scanner;
    std::string mock_filename = "(sge8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell8(" + t_sa + ") "
            "  alloc_cell8(" + t_sb + ") "
            "  alloc_cell8(" + t_tmp + ") "
            // extract sign bits
            "  copy8(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign8(" + t_sa + ") "
            "  copy8(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign8(" + t_sb + ") "
            // if signs differ, the negative one is smaller
            "  copy8(" + t_sa + ", " + t_tmp + ") "
            "  xor8(" + t_tmp + ", " + t_sb + ") "
            "  if(" + t_tmp + ") "
            "    copy8(" + t_sb + ", " + std::to_string(a) + ") "
            "  else "
            // if signs are the same, use unsigned comparison
            "    ge8(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "  endif "
            // free temp variables
            "  free_cell8(" + t_sa + ") "
            "  free_cell8(" + t_sb + ") "
            "  free_cell8(" + t_tmp + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_sge16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string t_sa = make_temp_name("t_sa");
    std::string t_sb = make_temp_name("t_sb");
    std::string t_tmp = make_temp_name("t_tmp");

    TokenScanner scanner;
    std::string mock_filename = "(sge16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // alloc temp variables
            "{ alloc_cell16(" + t_sa + ") "
            "  alloc_cell16(" + t_sb + ") "
            "  alloc_cell16(" + t_tmp + ") "
            // extract sign bits
            "  copy16(" + std::to_string(a) + ", " + t_sa + ") "
            "  sign16(" + t_sa + ") "
            "  copy16(" + std::to_string(b) + ", " + t_sb + ") "
            "  sign16(" + t_sb + ") "
            // if signs differ, the negative one is smaller
            "  copy16(" + t_sa + ", " + t_tmp + ") "
            "  xor16(" + t_tmp + ", " + t_sb + ") "
            "  if(" + t_tmp + ") "
            "    copy16(" + t_sb + ", " + std::to_string(a) + ") "
            "  else "
            // if signs are the same, use unsigned comparison
            "    ge16(" + std::to_string(a) + ", " + std::to_string(b) + ") "
            "  endif "
            // free temp variables
            "  free_cell16(" + t_sa + ") "
            "  free_cell16(" + t_sb + ") "
            "  free_cell16(" + t_tmp + ") "
            "}",
            mock_filename));

    return true;
}

bool MacroExpander::handle_shr8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_half = make_temp_name("T_half");
    std::string T_cmp = make_temp_name("T_cmp");
    std::string T_one = make_temp_name("T_one");
    std::string T_two = make_temp_name("T_two");
    std::string T_count = make_temp_name("T_count");

    TokenScanner scanner;
    std::string mock_filename = "(shr8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T_half + ") "
            "  alloc_cell8(" + T_cmp + ") "
            "  alloc_cell8(" + T_one + ") >" + T_one + " + "
            "  alloc_cell8(" + T_two + ") >" + T_two + " ++ "
            "  alloc_cell8(" + T_count + ") "
            // copy shift count to T_count
            "  copy8(" + std::to_string(b) + ", " + T_count + ") "
            "  repeat(" + T_count + ") "
            //   while (a >= 2), shift right
            "    copy8(" + std::to_string(a) + ", " + T_cmp + ") "
            "    ge8(" + T_cmp + ", " + T_two + ") "
            "    while(" + T_cmp + ") "
            "      sub8(" + std::to_string(a) + ", " + T_two + ") " // a -= 2
            "      add8(" + T_half + ", " + T_one + ") " // half += 1
            //     recompute condition for next iteration
            "      copy8(" + std::to_string(a) + ", " + T_cmp + ") "
            "      ge8(" + T_cmp + ", " + T_two + ") "
            "    endwhile "
            //   set the result
            "    move8(" + T_half + ", " + std::to_string(a) + ") "
            "  endrepeat "
            "  free_cell8(" + T_half + ") "
            "  free_cell8(" + T_cmp + ") "
            "  free_cell8(" + T_one + ") "
            "  free_cell8(" + T_two + ") "
            "  free_cell8(" + T_count + ") "
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

    std::string T_half = make_temp_name("T_half");
    std::string T_cmp = make_temp_name("T_cmp");
    std::string T_one = make_temp_name("T_one");
    std::string T_two = make_temp_name("T_two");
    std::string T_count = make_temp_name("T_count");

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

bool MacroExpander::handle_shl8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "expr_a", "expr_b" }, vals)) {
        return true;
    }
    int a = vals[0];
    int b = vals[1];

    std::string T_val = make_temp_name("T_val");
    std::string T_count = make_temp_name("T_count");

    TokenScanner scanner;
    std::string mock_filename = "(shl8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + T_val + ") "
            "  alloc_cell8(" + T_count + ") "
            // copy shift count to T_count
            "  copy8(" + std::to_string(b) + ", " + T_count + ") "
            "  repeat(" + T_count + ") "
            //   duplicate a
            "    copy8(" + std::to_string(a) + ", " + T_val + ") "
            "    add8(" + std::to_string(a) + ", " + T_val + ") "
            "  endrepeat "
            "  free_cell8(" + T_val + ") "
            "  free_cell8(" + T_count + ") "
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

    std::string T_val = make_temp_name("T_val");
    std::string T_count = make_temp_name("T_count");

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
    level.temp_if = make_temp_name("temp_if");
    level.temp_else = make_temp_name("temp_else");

    TokenScanner scanner;
    std::string mock_filename = "(if)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate temp variables
            "{ alloc_cell8(" + level.temp_if + ") "
            "  alloc_cell8(" + level.temp_else + ") "
            // copy cond to temp_else and negate it
            "  copy8(" + std::to_string(cond) + ", " + level.temp_else + ") "
            "  not8(" + level.temp_else + ")"
            // copy temp_else to temp_if and negate it
            "  copy8(" + level.temp_else + ", " + level.temp_if + ") "
            "  not8(" + level.temp_if + ") "
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
            "  free_cell8(" + level.temp_if + ") "
            "  free_cell8(" + level.temp_else + ") "
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
    level.temp_if = make_temp_name("temp_if");
    level.cond = cond;

    TokenScanner scanner;
    std::string mock_filename = "(while)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            // allocate temp variable
            "{ alloc_cell8(" + level.temp_if + ") "
            // copy cond to temp_if and negate it twice
            "  copy8(" + std::to_string(level.cond) + ", "
            + level.temp_if + ") "
            "  not8(" + level.temp_if + ")"
            "  not8(" + level.temp_if + ")"
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
            "  copy8(" + std::to_string(level.cond) + ", "
            + level.temp_if + ") "
            "  not8(" + level.temp_if + ")"
            "  not8(" + level.temp_if + ")"
            // re-enter the WHILE branch if temp_if == 1
            "  >" + level.temp_if + " "
            "  ] "
            // release temp variable
            "  free_cell8(" + level.temp_if + ") "
            "}",
            mock_filename));
    struct_stack_.pop_back();

    return true;
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

bool MacroExpander::handle_push8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "source_cell" }, vals)) {
        return true;
    }
    int source = vals[0];
    int target = parser.output().alloc_stack(2);

    TokenScanner scanner;
    std::string mock_filename = "(push8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "copy8(" + std::to_string(source) + ", " + std::to_string(target) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_push16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "source_cell" }, vals)) {
        return true;
    }
    int source = vals[0];
    int target = parser.output().alloc_stack(2);

    TokenScanner scanner;
    std::string mock_filename = "(push16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "copy16(" + std::to_string(source) + ", " + std::to_string(target) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_push8i(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "value" }, vals)) {
        return true;
    }
    int value = vals[0];
    int target = parser.output().alloc_stack(2);

    TokenScanner scanner;
    std::string mock_filename = "(push8i)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "set8(" + std::to_string(target) + ", " + std::to_string(value) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_push16i(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "value" }, vals)) {
        return true;
    }
    int value = vals[0];
    int target = parser.output().alloc_stack(2);

    TokenScanner scanner;
    std::string mock_filename = "(push16i)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "set16(" + std::to_string(target) + ", " + std::to_string(value) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_pop8(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "target_cell" }, vals)) {
        return true;
    }
    int target = vals[0];
    int source = parser.output().stack_ptr();
    parser.output().free_stack(2);

    TokenScanner scanner;
    std::string mock_filename = "(pop8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "move8(" + std::to_string(source) + ", " + std::to_string(target) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_pop16(Parser& parser, const Token& tok) {
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "target_cell" }, vals)) {
        return true;
    }
    int target = vals[0];
    int source = parser.output().stack_ptr();
    parser.output().free_stack(2);

    TokenScanner scanner;
    std::string mock_filename = "(pop16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "move16(" + std::to_string(source) + ", " + std::to_string(target) + ") ",
            mock_filename));
    return true;
}

bool MacroExpander::handle_alloc_global16(Parser& parser, const Token& tok) {
    Token func_tok = tok;
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "count" }, vals)) {
        return true;
    }
    int count16 = vals[0];
    int addr = parser.output().alloc_global(func_tok, count16);
    std::string clear_code = clear_memory_area(addr, count16);

    TokenScanner scanner;
    std::string mock_filename = "(alloc_global16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            clear_code,
            mock_filename));
    return true;
}

bool MacroExpander::handle_free_global16(Parser& parser, const Token&) {
    parser.advance(); // skip macro name
    parser.output().free_global();
    return true;
}

bool MacroExpander::handle_alloc_temp16(Parser& parser, const Token& tok) {
    Token func_tok = tok;
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "count" }, vals)) {
        return true;
    }
    int count16 = vals[0];
    int addr = parser.output().alloc_temp(func_tok, count16);
    std::string clear_code = clear_memory_area(addr, count16);

    TokenScanner scanner;
    std::string mock_filename = "(alloc_temp16)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            clear_code,
            mock_filename));
    return true;
}

bool MacroExpander::handle_free_temp16(Parser& parser, const Token&) {
    parser.advance(); // skip macro name
    parser.output().free_temp();
    return true;
}

bool MacroExpander::handle_enter_frame16(Parser& parser, const Token& tok) {
    Token func_tok = tok;
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "args16", "locals16"}, vals)) {
        return true;
    }
    int args16 = vals[0];
    int locals16 = vals[1];
    parser.output().enter_frame(func_tok, args16, locals16);
    return true;
}

bool MacroExpander::handle_leave_frame16(Parser& parser, const Token& tok) {
    Token func_tok = tok;
    parser.advance(); // skip macro name
    parser.output().leave_frame(func_tok);
    return true;
}

bool MacroExpander::handle_frame_alloc_temp16(Parser& parser, const Token& tok) {
    Token func_tok = tok;
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "temp16" }, vals)) {
        return true;
    }
    int temp16 = vals[0];
    parser.output().frame_alloc_temp(func_tok, temp16);
    return true;
}

bool MacroExpander::handle_print_char(Parser& parser, const Token& tok) {
    Token func_tok = tok;
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "char" }, vals)) {
        return true;
    }
    int ch = vals[0];

    std::string temp = make_temp_name("temp");

    TokenScanner scanner;
    std::string mock_filename = "(print_char)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ alloc_cell8(" + temp + ") "
            "  set8(" + temp + ", " + std::to_string(ch) + ") "
            "  >" + temp + " . "
            "  free_cell8(" + temp + ") "
            "}",
            mock_filename));
    return true;
}

bool MacroExpander::handle_print_char8(Parser& parser, const Token& tok) {
    Token func_tok = tok;
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "cell" }, vals)) {
        return true;
    }
    int cell = vals[0];

    std::string temp = make_temp_name("temp");

    TokenScanner scanner;
    std::string mock_filename = "(print_char8)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(
            "{ >" + std::to_string(cell) + " . }",
            mock_filename));
    return true;
}

bool MacroExpander::handle_print_string(Parser& parser, const Token& tok) {
    std::string text;
    if (!parse_string_arg(parser, tok, text)) {
        return true; // error already reported
    }

    std::string impl_code = "{ ";
    for (auto c : text) {
        impl_code += "print_char(" + std::to_string(static_cast<int>(c)) + ") ";
    }
    impl_code += "}";

    TokenScanner scanner;
    std::string mock_filename = "(print_string)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(impl_code, mock_filename));
    return true;
}

bool MacroExpander::handle_print_newline(Parser& parser, const Token&) {
    parser.advance(); // consume macro name

    TokenScanner scanner;
    std::string mock_filename = "(print_newline)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string("print_char(10)", mock_filename));
    return true;
}

bool MacroExpander::handle_print_cellX(Parser& parser, const Token& tok,
                                       int width) {
    assert(width == 8 || width == 16);
    std::string X = std::to_string(width);
    const int max_digits = (width == 8) ? 3 : 5;

    Token func_tok = tok;
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "cell" }, vals)) {
        return true;
    }
    int a = vals[0];

    // temps
    std::string t_a = make_temp_name("t_a");
    std::string t_digit = make_temp_name("t_digit");
    std::string t_cond = make_temp_name("t_cond");
    std::string t_10 = make_temp_name("t_10");
    std::string t_0_char = make_temp_name("t_0_char");

    // numbers for digit buffer indices
    std::vector<std::string> t_numbers;
    for (int i = 0; i < max_digits; i++) {
        t_numbers.push_back(make_temp_name("number_" + std::to_string(i)));
    }

    // buffer for digits
    std::string t_idx = make_temp_name("t_idx");
    std::vector<std::string> t_buffer;
    for (int i = 0; i < max_digits; i++) {
        t_buffer.push_back(make_temp_name("buffer_" + std::to_string(i)));
    }

    // allocate and initialize temps
    std::string impl;
    impl =
        "{ alloc_cell" + X + "(" + t_a + ") "
        "  alloc_cell" + X + "(" + t_digit + ") "
        "  alloc_cell" + X + "(" + t_cond + ") "
        "  alloc_cell" + X + "(" + t_10 + ") "
        "  set" + X + "(" + t_10 + ", 10) "
        "  alloc_cell" + X + "(" + t_0_char + ") "
        "  set" + X + "(" + t_0_char + ", '0') ";

    for (int i = 0; i < max_digits; i++) {
        impl +=
            "  alloc_cell" + X + "(" + t_numbers[i] + ") "
            "  set" + X + "(" + t_numbers[i] + ", " + std::to_string(i) + ") ";
    }

    impl +=
        "  alloc_cell8(" + t_idx + ") ";

    for (int i = 0; i < max_digits; i++) {
        impl +=
            "  alloc_cell8(" + t_buffer[i] + ") ";
    }

    // copy a
    impl +=
        "  copy" + X + "(" + std::to_string(a) + ", " + t_a + ") ";

    // loop runs at least once to print "0"
    impl +=
        "  set" + X + "(" + t_cond + ", 1) "
        "  while(" + t_cond + ") "
        //   compute next digit
        "    copy" + X + "(" + t_a + ", " + t_digit + ") "
        "    mod" + X + "(" + t_digit + ", " + t_10 + ") "
        "    add" + X + "(" + t_digit + ", " + t_0_char + ") ";

    // store next digit
    for (int i = 0; i < max_digits; i++) {
        impl +=
            "    copy8(" + t_idx + ", " + t_cond + ") "
            "    eq8(" + t_cond + ", " + t_numbers[i] + ") "
            "    if(" + t_cond + ") "
            "      copy8(" + t_digit + ", " + t_buffer[i] + ") "
            "    endif ";
    }
    impl +=
        "    add8(" + t_idx + ", " + t_numbers[1] + ") ";

    impl +=
        //    divide t_a by 10 and set the loop condition
        "    div" + X + "(" + t_a + ", " + t_10 + ") "
        "    copy" + X + "(" + t_a + ", " + t_cond + ") "
        "    ne" + X + "(" + t_cond + ", " + t_numbers[0] + ") "
        "  endwhile ";

    // now print all digits on the stack in the correct order
    impl +=
        "  sub8(" + t_idx + ", " + t_numbers[1] + ") "
        "  set8(" + t_cond + ", 1) "
        "  while(" + t_cond + ") ";

    // print digit[idx]
    for (int i = 0; i < max_digits; i++) {
        impl +=
            "    copy8(" + t_idx + ", " + t_cond + ") "
            "    eq8(" + t_cond + ", " + t_numbers[i] + ") "
            "    if(" + t_cond + ") "
            "      print_char8(" + t_buffer[i] + ") "
            "    endif ";
    }

    impl +=
        "    copy8(" + t_idx + ", " + t_cond + ") "
        "    ne8(" + t_cond + ", " + t_numbers[0] + ") "
        "    sub8(" + t_idx + ", " + t_numbers[1] + ") "
        "  endwhile "
        "  print_char(' ') ";

    // free temps
    impl +=
        "  free_cell" + X + "(" + t_a + ") "
        "  free_cell" + X + "(" + t_digit + ") "
        "  free_cell" + X + "(" + t_cond + ") "
        "  free_cell" + X + "(" + t_10 + ") "
        "  free_cell" + X + "(" + t_0_char + ") ";

    for (int i = 0; i < max_digits; i++) {
        impl +=
            "  free_cell" + X + "(" + t_numbers[i] + ") ";
    }

    impl +=
        "  free_cell8(" + t_idx + ") ";

    for (int i = 0; i < max_digits; i++) {
        impl +=
            "  free_cell8(" + t_buffer[i] + ") ";
    }

    impl +=
        "} ";

    TokenScanner scanner;
    std::string mock_filename = "(print_cell" + X + ")";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(impl, mock_filename));
    return true;
}

bool MacroExpander::handle_print_cell8(Parser& parser, const Token& tok) {
    return handle_print_cellX(parser, tok, 8);
}

bool MacroExpander::handle_print_cell16(Parser& parser, const Token& tok) {
    return handle_print_cellX(parser, tok, 16);
}

bool MacroExpander::handle_print_cellXs(Parser& parser, const Token& tok,
                                        int width) {
    assert(width == 8 || width == 16);
    std::string X = std::to_string(width);

    Token func_tok = tok;
    std::vector<int> vals;
    if (!parse_expr_args(parser, tok, { "cell" }, vals)) {
        return true;
    }
    int a = vals[0];

    // temps
    std::string t_a = make_temp_name("t_a");
    std::string t_sign = make_temp_name("t_sign");

    // allocate and initialize temps
    std::string impl;
    impl =
        "{ alloc_cell" + X + "(" + t_a + ") "
        "  alloc_cell" + X + "(" + t_sign + ") ";

    // copy a
    impl +=
        "  copy" + X + "(" + std::to_string(a) + ", " + t_a + ") ";

    // print sign if negative
    impl +=
        "  copy" + X + "(" + std::to_string(a) + ", " + t_sign + ") "
        "  sign" + X + "(" + t_sign + ") "
        "  if(" + t_sign + ") " // if negative
        "    print_char('-') "
        "    abs" + X + "(" + t_a + ") "
        "  endif ";

    // call unsigned print
    impl +=
        "  print_cell" + X + "(" + t_a + ") ";

    // free temps
    impl +=
        "  free_cell" + X + "(" + t_a + ") "
        "  free_cell" + X + "(" + t_sign + ") "
        "} ";

    TokenScanner scanner;
    std::string mock_filename = "(print_cell" + X + "s)";
    parser.push_macro_expansion(
        mock_filename,
        scanner.scan_string(impl, mock_filename));
    return true;
}

bool MacroExpander::handle_print_cell8s(Parser& parser, const Token& tok) {
    return handle_print_cellXs(parser, tok, 8);
}

bool MacroExpander::handle_print_cell16s(Parser& parser, const Token& tok) {
    return handle_print_cellXs(parser, tok, 16);
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
        ExpressionParser expr(source, parser_, /*undefined_as_zero=*/false);
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

bool MacroExpander::parse_string_arg(Parser& parser, const Token& tok,
                                     std::string& text_out) {
    // Save name before collect_args moves parser.current_
    const std::string macro_name = tok.text;

    Macro fake;
    fake.name = macro_name;
    fake.params = { "string" };

    std::vector<std::vector<Token>> args;
    if (!collect_args(parser, fake, args)) {
        return false; // error already reported
    }

    if (args.size() != 1 || args[0].size() != 1 ||
            args[0][0].type != TokenType::String) {
        g_error_reporter.report_error(tok.loc,
                                      "macro '" + macro_name + "' expects one string");
        return false;
    }

    text_out = args[0][0].text;
    if (!text_out.empty() and text_out.front() == '"') {
        text_out.erase(0);
    }
    if (!text_out.empty() and text_out.back() == '"') {
        text_out.pop_back();
    }

    return true;
}

std::string MacroExpander::clear_memory_area(int addr, int size16) {
    std::string code_impl = "{ >" + std::to_string(addr) + " ";
    for (int i = 0; i < size16 * 2; ++i) {
        code_impl += "[-] > ";
    }
    code_impl += "}";
    return code_impl;
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
           MacroExpander::is_builtin_name(name) ||
           ExpressionParser::is_function_name(name);
}

// temporary names generated for macro expansions
static int g_temp_counter = 0;

std::string make_temp_name(const std::string& suffix) {
    return "_T" + std::to_string(++g_temp_counter) + "_" + suffix;
}

void reset_temp_names() {
    g_temp_counter = 0;
}
