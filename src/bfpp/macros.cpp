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
            return {};
        }

        parser.advance(); // consume comma
    }

    // After parsing expected args, expect ')'
    if (parser.current_.type != TokenType::RParen) {
        g_error_reporter.report_error(
            parser.current_.loc,
            "expected ')' at end of macro call"
        );
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
           name == "end" ||
           name == "undef";
}

bool contains_directive_token(const std::vector<Token>& body) {
    for (const Token& t : body) {
        if (t.type == TokenType::Directive) {
            // Any '#' token is forbidden in macro bodies
            return true;
        }
    }
    return false;
}


#if 0

void MacroExpander::expand_token(const Token& tok,
                                 std::deque<Token>& out) {
    // Object-like macros
    if (tok.type == TokenType::Identifier) {
        const Macro* m = g_macro_table.lookup(tok.text);
        if (m && m->params.empty()) {
            expand_macro_body(*m, m->body, out);
            return;
        }
    }

    // Extended BF instructions (global)
    if (tok.type == TokenType::BFInstr) {
        TokenStream in(tok);
        if (try_expand_extended_bf(in, out)) {
            return;
        }

        emit_bf_token(tok, out);
        return;
    }

    // Everything else
    out.push_back(tok);
}

void MacroExpander::expand_macro_body(const Macro& m,
                                      const std::vector<Token>& body,
                                      std::deque<Token>& out) {
    int start_pos = pointer_pos_;

    TokenStream in(body);

    while (!in.eof()) {
        // Try extended BF first
        if (try_expand_extended_bf(in, out)) {
            continue;
        }

        // Otherwise emit raw token
        const Token& t = in.peek(0);

        if (t.type == TokenType::BFInstr) {
            emit_bf_token(t, out); // updates pointer_pos_
        }
        else {
            out.push_back(t);
        }

        in.consume();
    }

    if (pointer_pos_ != start_pos) {
        g_error_reporter.report_warning(
            m.loc,
            "macro '" + m.name + "' leaves pointer at " +
            std::to_string(pointer_pos_) +
            " (was " + std::to_string(start_pos) + ")"
        );
    }
}

bool MacroExpander::try_expand_extended_bf(TokenStream& in,
        std::deque<Token>& out) {
    const Token& op = in.peek(0);

    if (op.type != TokenType::BFInstr ||
            (op.text != ">" && op.text != "<" &&
             op.text != "+" && op.text != "-")) {
        return false;
    }

    const Token& next = in.peek(1);
    if (next.type == TokenType::EndOfFile) {
        return false;
    }

    int value = 0;
    bool ok = false;

    // Case 1: op number
    if (next.type == TokenType::Integer) {
        value = next.int_value;
        ok = true;
        in.consume(2); // op + number
    }

    // Case 2: op IDENT
    else if (next.type == TokenType::Identifier) {
        std::vector<Token> expr_tokens = resolve_ident_to_expr_tokens(next);
        value = eval_expr_tokens(expr_tokens, next.loc);
        ok = true;
        in.consume(2); // op + ident
    }

    // Case 3: op (expr)
    else if (next.type == TokenType::LParen) {
        std::vector<Token> expr_tokens;
        int depth = 0;
        size_t j = 2; // skip op + '('

        for (;; ++j) {
            const Token& t = in.peek(j);
            if (t.type == TokenType::EndOfFile) {
                return false;
            }

            if (t.type == TokenType::LParen) {
                depth++;
                expr_tokens.push_back(t);
                continue;
            }

            if (t.type == TokenType::RParen) {
                if (depth == 0) {
                    break;
                }
                depth--;
                expr_tokens.push_back(t);
                continue;
            }

            expr_tokens.push_back(t);
        }

        // j is the index of ')'
        value = eval_expr_tokens(expr_tokens, next.loc);
        ok = true;
        in.consume(j + 1); // consume op, '(', expr..., ')'
    }

    if (!ok) {
        return false;
    }

    apply_extended_bf_op(op, value, out);
    return true;
}

bool MacroExpander::expand_extended_bf(const std::vector<Token>& body,
                                       size_t& i,
                                       std::deque<Token>& out) {
    const Token& op = body[i];
    if (i + 1 >= body.size()) {
        return false;
    }

    const Token& next = body[i + 1];

    int value = 0;
    bool ok = false;

    // op number
    if (next.type == TokenType::Integer) {
        value = next.int_value;
        ok = true;
        i += 1;
    }
    // op IDENT
    else if (next.type == TokenType::Identifier) {
        std::vector<Token> expr_tokens = resolve_ident_to_expr_tokens(next);
        value = eval_expr_tokens(expr_tokens, next.loc);
        ok = true;
        i += 1;
    }
    // op (expr)
    else if (next.type == TokenType::LParen) {
        std::vector<Token> expr_tokens;
        int depth = 0;
        size_t j = i + 1;

        while (++j < body.size()) {
            const Token& u = body[j];
            if (u.type == TokenType::LParen) {
                depth++;
                expr_tokens.push_back(u);
                continue;
            }
            if (u.type == TokenType::RParen) {
                if (depth == 0) {
                    break;
                }
                depth--;
                expr_tokens.push_back(u);
                continue;
            }
            expr_tokens.push_back(u);
        }

        if (j >= body.size() || body[j].type != TokenType::RParen) {
            return false;    // malformed, let normal path handle
        }

        value = eval_expr_tokens(expr_tokens, next.loc);
        ok = true;
        i = j; // consume up to ')'
    }

    if (!ok) {
        return false;
    }

    apply_extended_bf_op(op, value, out);
    return true;
}

int MacroExpander::eval_expr_tokens(const std::vector<Token>& expr,
                                    const SourceLocation& loc) {
    struct Stream {
        const std::vector<Token>& v;
        SourceLocation loc;
        size_t pos = 0;
        const Token& peek() const {
            static Token eof(TokenType::EndOfFile, "", loc);
            return pos < v.size() ? v[pos] : eof;
        }
        const Token& next() {
            const Token& t = peek();
            if (pos < v.size()) {
                ++pos;
            }
            return t;
        }
    } s{ expr, loc };

    std::function<int()> parse_expr, parse_term, parse_factor;

    parse_factor = [&]() -> int {
        const Token& t = s.peek();
        if (t.type == TokenType::Integer) {
            s.next();
            return t.int_value;
        }
        if (t.type == TokenType::LParen) {
            s.next();
            int v = parse_expr();
            if (s.peek().type != TokenType::RParen) {
                g_error_reporter.report_error(loc, "missing ')'");
                return 0;
            }
            s.next();
            return v;
        }
        g_error_reporter.report_error(loc, "expected number or '(' in expression");
        return 0;
    };

    parse_term = [&]() -> int {
        int v = parse_factor();
        while (true) {
            const Token& t = s.peek();
            if (t.type == TokenType::Operator &&
                    (t.text == "*" || t.text == "/" || t.text == "%")) {
                s.next();
                int rhs = parse_factor();
                if (t.text == "*") {
                    v *= rhs;
                }
                else if (t.text == "/") {
                    v /= rhs;
                }
                else {
                    v %= rhs;
                }
            }
            else {
                break;
            }
        }
        return v;
    };

    parse_expr = [&]() -> int {
        int v = parse_term();
        while (true) {
            const Token& t = s.peek();
            if (t.type == TokenType::Operator &&
                    (t.text == "+" || t.text == "-")) {
                s.next();
                int rhs = parse_term();
                if (t.text == "+") {
                    v += rhs;
                }
                else {
                    v -= rhs;
                }
            }
            else {
                break;
            }
        }
        return v;
    };

    int result = parse_expr();
    if (s.peek().type != TokenType::EndOfFile) {
        g_error_reporter.report_error(loc, "extra tokens in expression");
    }
    return result;
}

void MacroExpander::emit_bf_token(const Token& t,
                                  std::deque<Token>& out) {
    if (t.type == TokenType::BFInstr) {
        if (t.text == ">") {
            pointer_pos_++;
        }
        else if (t.text == "<") {
            pointer_pos_--;
            if (pointer_pos_ < 0) {
                g_error_reporter.report_error(t.loc, "pointer underflow");
                exit(EXIT_FAILURE);
            }
        }
    }
    out.push_back(t);
}

void MacroExpander::apply_extended_bf_op(const Token& op,
        int value,
        std::deque<Token>& out) {
    char c = op.text[0];

    if (c == '+' || c == '-') {
        char instr = (c == '+') ? '+' : '-';
        if (value < 0) {
            instr = (instr == '+') ? '-' : '+';
            value = -value;
        }
        for (int k = 0; k < value; ++k) {
            out.push_back(Token::make_bf(instr, op.loc));
        }
        return;
    }

    if (c == '>' || c == '<') {
        int target = value;
        int delta = target - pointer_pos_;
        char instr = '>';
        if (delta < 0) {
            instr = '<';
            delta = -delta;
        }
        for (int k = 0; k < delta; ++k) {
            out.push_back(Token::make_bf(instr, op.loc));
        }
        pointer_pos_ = target;
        return;
    }

    // Fallback: emit op as-is
    out.push_back(op);
}

std::vector<Token> MacroExpander::resolve_ident_to_expr_tokens(const Token& ident) {
    const Macro* m = g_macro_table.lookup(ident.text);
    if (!m) {
        g_error_reporter.report_error(
            ident.loc,
            "identifier '" + ident.text +
            "' cannot be used in extended BF operator (not a macro)"
        );
        return { Token::make_int(0, ident.loc) };
    }

    if (!m->params.empty()) {
        g_error_reporter.report_error(
            ident.loc,
            "function-like macro '" + ident.text +
            "' cannot be used as a numeric expression"
        );
        return { Token::make_int(0, ident.loc) };
    }

    // Object-like macro: its body is the expression
    return m->body;
}

bool token_allowed_in_expression(const Token& t) {
    switch (t.type) {
    case TokenType::Integer:
    case TokenType::Identifier:
    case TokenType::Operator:
    case TokenType::LParen:
    case TokenType::RParen:
        return true;

    default:
        return false;
    }
}

bool all_tokens_allowed_in_expression(const std::vector<Token>& tokens) {
    for (const Token& t : tokens) {
        if (!token_allowed_in_expression(t)) {
            return false;
        }
    }
    return true;
}

#endif
