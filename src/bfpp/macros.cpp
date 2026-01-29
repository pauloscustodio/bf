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

struct Builtin {
    const char* name;
    bool (*handler)(Parser& parser, const Token& tok);
};

static bool handle_alloc_cell(Parser& parser, const Token& tok) {
    // Expect alloc_cell(<ident>)
    Macro fake;
    fake.name = tok.text;
    fake.params = { "name" };
    std::vector<std::vector<Token>> args =
        parser.macro_expander().collect_args(parser, fake);

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
    return true;
}

static bool handle_free_cell(Parser& parser, const Token& tok) {
    // Expect free_cell(<ident>)
    Macro fake;
    fake.name = tok.text;
    fake.params = { "name" };
    std::vector<std::vector<Token>> args =
        parser.macro_expander().collect_args(parser, fake);

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
    return true;
}

static const Builtin kBuiltins[] = {
    { "alloc_cell", handle_alloc_cell },
    { "free_cell",  handle_free_cell  },
};

static bool is_builtin_macro(const std::string& name) {
    for (const auto& b : kBuiltins) {
        if (name == b.name) {
            return true;
        }
    }
    return false;
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

    for (const auto& b : kBuiltins) {
        if (token.text == b.name) {
            // Builtins consume what they need; no expansion frame.
            b.handler(parser, token);
            parser.suppress_next_advance_ = true; // don't advance in parse_statement
            return true;
        }
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
    std::vector<std::vector<Token>> args = collect_args(parser, *macro);

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
    MacroExpansionFrame frame;
    frame.tokens = std::move(expanded);
    frame.index = 0;
    parser.expansion_stack_.push_back(std::move(frame));

    // expanding_.erase() called in Parser::advance() when popping frame

    return true;
}

void MacroExpander::remove_expanding(const std::string& name) {
    expanding_.erase(name);
}

std::vector<std::vector<Token>> MacroExpander::collect_args(Parser& parser,
const Macro& macro) {
    std::vector<std::vector<Token>> actuals;

    auto consume_to_eol = [&]() {
        while (parser.current_.type != TokenType::EndOfLine &&
               parser.current_.type != TokenType::EndOfInput) {
            parser.advance();
        }
    };

    // Object-like macro: no arguments expected
    if (macro.params.empty()) {
        parser.advance(); // consume macro name (no args to collect)
        return actuals;
    }

    // Function-like macro: expect '(' after the macro name
    parser.advance(); // consume macro name

    if (parser.current_.type != TokenType::LParen) {
        g_error_reporter.report_error(
            parser.current_.loc,
            "expected '(' after macro name '" + macro.name + "'"
        );
        consume_to_eol();
        return {};
    }

    parser.advance(); // consume '('

    // Special case: empty argument list "FOO()"
    if (parser.current_.type == TokenType::RParen) {
        parser.advance(); // consume ')'
        return actuals; // mismatch handled by caller
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
                return {};
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
                    return {};
                }
            }

            arg_tokens.push_back(parser.current_);
            parser.advance();
        }

        actuals.push_back(std::move(arg_tokens));

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
            return {};
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
        return {};
    }

    parser.advance(); // consume ')'
    return actuals;
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
        is_builtin_macro(name);
}
