//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "expr.h"
#include "lexer.h"
#include "macros.h"
#include "parser.h"

Parser::Parser(Lexer& lexer)
    : lexer_(lexer), macro_expander_(g_macro_table) {
}

bool Parser::run(std::string& output_) {
    advance();  // get first token
    if (parse()) {
        optimize_tape_movements();
        output_ = to_string();
        return true;
    }
    else {
        output_.clear();
        return false;
    }
}

const Token& Parser::current() const {
    return current_;
}

Token Parser::peek(size_t offset) {
    size_t remaining = offset;

    // Walk from top to bottom, skipping exhausted frames
    for (auto it = expansion_stack_.rbegin(); it != expansion_stack_.rend(); ++it) {
        size_t available = (it->index < it->tokens.size())
                           ? (it->tokens.size() - it->index)
                           : 0;
        if (remaining < available) {
            return it->tokens[it->index + remaining];
        }
        remaining -= available;
    }

    // Not found in expansion stack, peek from lexer
    return lexer_.peek(remaining);
}

void Parser::advance() {
    // Pop exhausted frames AND remove from expanding set
    while (!expansion_stack_.empty() &&
            expansion_stack_.back().index >= expansion_stack_.back().tokens.size()) {
        // Clean up the recursion guard
        macro_expander_.remove_expanding(expansion_stack_.back().macro_name);
        expansion_stack_.pop_back();
    }

    // Try to consume from the top frame (if any)
    if (!expansion_stack_.empty()) {
        auto& frame = expansion_stack_.back();
        current_ = frame.tokens[frame.index++];
        return;
    }

    // No expansion frames, get from lexer
    current_ = lexer_.get();
}

void Parser::push_macro_expansion(const std::string& name, const std::vector<Token>& tokens) {
    MacroExpansionFrame frame;
    frame.macro_name = name;
    frame.tokens = tokens;           // expansion body
    frame.tokens.push_back(current_); // resume with the token we had already loaded
    frame.index = 0;
    expansion_stack_.push_back(std::move(frame));
}

MacroExpander& Parser::macro_expander() {
    return macro_expander_;
}

BFOutput& Parser::output() {
    return output_;
}

void Parser::set_stack_base(int base) {
    output_.set_stack_base(base);
}

int Parser::heap_size() const {
    return output_.heap_size();
}

int Parser::max_stack_depth() const {
    return output_.max_stack_depth();
}

void Parser::optimize_tape_movements() {
    output_.optimize_tape_movements();
}

std::string Parser::to_string() const {
    return output_.to_string();
}

bool Parser::parse() {
    while (current_.type != TokenType::EndOfInput) {
        if (current_.type == TokenType::EndOfLine) {
            advance();
            continue;
        }

        if (current_.type == TokenType::Directive) {
            parse_directive();
        }
        else {
            // only execute when all active #if conditions are true
            if (if_branch_active()) {
                parse_statements();
            }
            else {
                skip_to_end_of_line();
            }
        }
    }

    // After finishing, check for unclosed builtin structures
    macro_expander_.check_struct_stack();

    // After finishing, check for unmatched loops
    output_.check_loops();

    // After finishing, check for unmatched braces
    for (const BraceFrame& frame : brace_stack_) {
        g_error_reporter.report_error(
            frame.loc,
            "unmatched '{' brace"
        );
    }

    // After finishing the file, check for unclosed #if blocks
    if (!if_stack_.empty()) {
        const IfState& state = if_stack_.back();
        g_error_reporter.report_error(
            state.loc,  // store loc when pushing IfState
            "unterminated #if (missing #endif)"
        );
    }

    return !g_error_reporter.has_errors();
}

void Parser::parse_directive() {
    Token directive = current_;
    advance(); // consume directive keyword

    if (directive.text == "#include") {
        if (if_branch_active()) {
            parse_include();
        }
    }
    else if (directive.text == "#define") {
        if (if_branch_active()) {
            parse_define();
        }
    }
    else if (directive.text == "#undef") {
        if (if_branch_active()) {
            parse_undef();
        }
    }
    else if (directive.text == "#if") {
        parse_if();
    }
    else if (directive.text == "#elsif") {
        parse_elsif();
    }
    else if (directive.text == "#else") {
        parse_else();
    }
    else if (directive.text == "#endif") {
        parse_endif();
    }
    else {
        g_error_reporter.report_error(
            directive.loc,
            "unknown directive: '" + directive.text + "'"
        );
        skip_to_end_of_line();
    }

    if (if_branch_active()) {
        if (current_.type != TokenType::EndOfLine &&
                current_.type != TokenType::EndOfInput) {
            g_error_reporter.report_error(
                current_.loc,
                "unexpected token after " + directive.text +
                ": '" + current_.text + "'"
            );
        }
    }

    skip_to_end_of_line();
}

void Parser::parse_include() {
    if (current_.type != TokenType::String) {
        g_error_reporter.report_error(
            current_.loc,
            "expected string literal after #include"
        );
        skip_to_end_of_line();
        return;
    }

    std::string filename = current_.text;
    advance();

    if (!g_file_stack.push_file(filename, current_.loc)) {
        // error already reported
        return;
    }
}

void Parser::parse_define() {
    SourceLocation define_loc = current_.loc;

    // Expect macro name
    if (current_.type != TokenType::Identifier) {
        g_error_reporter.report_error(current_.loc, "expected macro name");
        skip_to_end_of_line();
        return;
    }

    std::string name = current_.text;
    SourceLocation name_loc = current_.loc;
    advance(); // consume name

    // Reserved keyword check
    if (is_reserved_keyword(name)) {
        g_error_reporter.report_error(
            name_loc,
            "cannot define macro '" + name + "': reserved word"
        );
        skip_to_end_of_line();
        return;
    }

    std::vector<std::string> params;
    std::vector<Token> body;

    // Function-like macro?
    if (current_.type == TokenType::LParen) {
        // Parse parameter list
        advance(); // consume '('

        if (current_.type != TokenType::RParen) {
            while (true) {
                if (current_.type != TokenType::Identifier) {
                    g_error_reporter.report_error(
                        current_.loc,
                        "expected parameter name"
                    );
                    skip_to_end_of_line();
                    return;
                }

                // Reserved keyword check
                if (is_reserved_keyword(current_.text)) {
                    g_error_reporter.report_error(
                        current_.loc,
                        "cannot define parameter '" + current_.text + "': reserved word"
                    );
                    skip_to_end_of_line();
                    return;
                }

                params.push_back(current_.text);
                advance(); // consume parameter

                if (current_.type == TokenType::RParen) {
                    break;
                }

                if (!current_.is_comma()) {
                    g_error_reporter.report_error(
                        current_.loc,
                        "expected ',' or ')'"
                    );
                    skip_to_end_of_line();
                    return;
                }

                advance(); // consume comma
            }
        }

        advance(); // consume ')'
    }

    // Collect the replacement list until end of line (object-like or function-like)
    while (current_.type != TokenType::EndOfLine &&
            current_.type != TokenType::EndOfInput) {
        body.push_back(current_);
        advance();
    }

    // Duplicate parameter validation
    for (std::size_t i = 0; i + 1 < params.size(); ++i) {
        for (std::size_t j = i + 1; j < params.size(); ++j) {
            if (params[i] == params[j]) {
                g_error_reporter.report_error(
                    define_loc,
                    "duplicate parameter name '" + params[i] +
                    "' in macro '" + name + "'"
                );
                return;
            }
        }
    }

    // Store macro
    Macro macro;
    macro.name = name;
    macro.loc = name_loc;
    macro.params = params;
    macro.body = body;
    g_macro_table.define(macro);
}

void Parser::parse_undef() {
    if (current_.type != TokenType::Identifier) {
        g_error_reporter.report_error(current_.loc, "expected macro name");
        return;
    }

    std::string name = current_.text;
    if (is_reserved_keyword(name)) {
        g_error_reporter.report_error(
            current_.loc,
            "cannot undefine reserved word '" + name + "'"
        );
        advance();
        return;
    }

    g_macro_table.undef(name);
    advance();
}

void Parser::parse_if() {
    SourceLocation loc = current_.loc;   // location of #if

    if (current_.type == TokenType::EndOfLine) {
        g_error_reporter.report_error(loc, "missing expression after #if");
        return;
    }

    // Evaluate expression
    ParserTokenSource ts(*this);
    ExpressionParser expr(ts, /*undefined_as_zero=*/true);
    int value = expr.parse_expression();

    IfState state;
    state.condition_true = (value != 0);
    state.branch_taken = state.condition_true;
    state.in_else = false;
    state.loc = loc;

    if_stack_.push_back(state);
}

void Parser::parse_elsif() {
    SourceLocation loc = current_.loc;   // location of #elsif

    if (current_.type == TokenType::EndOfLine) {
        g_error_reporter.report_error(loc, "missing expression after #elsif");
        return;
    }

    // Evaluate expression
    ParserTokenSource ts(*this);
    ExpressionParser expr(ts, /*undefined_as_zero=*/true);
    int value = expr.parse_expression();

    if (if_stack_.empty()) {
        g_error_reporter.report_error(loc, "#elsif without matching #if");
        return;
    }

    IfState& state = if_stack_.back();

    if (state.in_else) {
        g_error_reporter.report_error(loc, "#elsif after #else");
        return;
    }

    if (state.branch_taken) {
        // A previous branch was already taken; this branch is inactive
        state.condition_true = false;
    }
    else {
        state.condition_true = (value != 0);
        state.branch_taken = state.condition_true;
    }
}

void Parser::parse_else() {
    SourceLocation loc = current_.loc;

    if (if_stack_.empty()) {
        g_error_reporter.report_error(loc, "#else without matching #if");
        return;
    }

    IfState& state = if_stack_.back();

    if (state.in_else) {
        g_error_reporter.report_error(loc, "multiple #else in the same #if");
        return;
    }

    state.in_else = true;

    if (state.branch_taken) {
        // A previous branch already executed; this else is inactive
        state.condition_true = false;
    }
    else {
        state.condition_true = true;
        state.branch_taken = true;
    }
}

void Parser::parse_endif() {
    SourceLocation loc = current_.loc;

    if (if_stack_.empty()) {
        g_error_reporter.report_error(loc, "#endif without matching #if");
        return;
    }

    if_stack_.pop_back();
}

void Parser::parse_statements() {
    while (current_.type != TokenType::EndOfLine &&
            current_.type != TokenType::EndOfInput) {
        parse_statement();
    }
}

void Parser::parse_statement() {
    // Try macro expansion first
    while (macro_expander_.try_expand(*this, current_)) {
        advance(); // load first token from expansion or move past macro name
    }

    if (current_.type == TokenType::EndOfLine ||
            current_.type == TokenType::EndOfInput) {
        return;
    }

    if (current_.type == TokenType::LBrace) {
        parse_left_brace();
        return;
    }

    if (current_.type == TokenType::RBrace) {
        parse_right_brace();
        return;
    }

    if (current_.type == TokenType::BFInstr) {
        parse_bfinstr();
        return;
    }

    g_error_reporter.report_error(
        current_.loc,
        "unexpected token in statement: '" + current_.text + "'"
    );
    advance(); // consume unexpected token
}

void Parser::parse_bfinstr() {
    Token op_tok = current_;
    char op = op_tok.text[0];
    advance(); // consume BFInstr

    switch (op) {
    case '+':
    case '-':
        parse_bf_plus_minus(op_tok);
        break;
    case '<':
    case '>':
        parse_bf_left_right(op_tok);
        break;
    case '[':
        parse_bf_loop_start(op_tok);
        break;
    case ']':
        parse_bf_loop_end(op_tok);
        break;
    case ',':
        parse_bf_input(op_tok);
        break;
    case '.':
        parse_bf_output(op_tok);
        break;
    default:
        g_error_reporter.report_error(
            op_tok.loc,
            "invalid Brainfuck instruction: '" + op_tok.text + "'"
        );
    }
}

void Parser::parse_bf_plus_minus(const Token& tok) {
    int count = 1;
    int arg = 0;
    if (parse_bf_int_arg(arg)) {
        count = arg;
    }
    output_count_bf_instr(tok, count);
}

void Parser::parse_bf_left_right(const Token& tok) {
    int count = 1;
    int pos = 0;
    if (parse_bf_int_arg(pos)) {
        if (tok.text[0] == '>') {
            count = pos - output_.tape_ptr();
        }
        else {
            count = output_.tape_ptr() - pos;
        }
    }
    output_count_bf_instr(tok, count);
}

void Parser::parse_bf_loop_start(const Token& tok) {
    int pos = output_.tape_ptr();
    loop_stack_.push_back(LoopFrame{ tok.loc, pos });
    output_count_bf_instr(tok, 1);
}

void Parser::parse_bf_loop_end(const Token& tok) {
    if (loop_stack_.empty()) {
        g_error_reporter.report_error(
            tok.loc,
            "unmatched ']' instruction"
        );
        return;
    }
    if (loop_stack_.back().tape_ptr_at_start != output_.tape_ptr()) {
        g_error_reporter.report_error(
            tok.loc,
            "tape pointer mismatch at ']' instruction (expected " +
            std::to_string(loop_stack_.back().tape_ptr_at_start) +
            ", got " + std::to_string(output_.tape_ptr()) + ")"
        );
        g_error_reporter.report_note(
            loop_stack_.back().loc,
            "corresponding '[' instruction here"
        );
    }
    loop_stack_.pop_back();
    output_count_bf_instr(tok, 1);
}

void Parser::parse_bf_input(const Token& tok) {
    output_count_bf_instr(tok, 1);
}

void Parser::parse_bf_output(const Token& tok) {
    output_count_bf_instr(tok, 1);
}

void Parser::output_count_bf_instr(const Token& tok, int count) {
    char op = tok.text[0];
    if (count < 0) {
        // invert direction
        switch (op) {
        case '<':
            op = '>';
            break;
        case '>':
            op = '<';
            break;
        case '+':
            op = '-';
            break;
        case '-':
            op = '+';
            break;
        default:
            g_error_reporter.report_error(
                tok.loc,
                "cannot invert Brainfuck instruction: '" + tok.text + "'"
            );
            return;
        }
        count = -count;
    }
    for (int i = 0; i < count; ++i) {
        output_.put(Token::make_bf(op, tok.loc));
    }
}

bool Parser::parse_bf_int_arg(int& output) {
    if (current_.type == TokenType::Integer) {
        output = current_.int_value;
        advance();      // consume integer
        return true;
    }

    if (current_.type == TokenType::Identifier &&
            !is_reserved_keyword(current_.text)) {
        // evaluate expression result of macro expansion
        std::vector<Token> expr_tokens{ current_ };
        ArrayTokenSource source(expr_tokens);
        ExpressionParser expr(source);
        output = expr.parse_expression();
        advance(); // consume identifier
        return true;
    }

    if (current_.type == TokenType::LParen) {
        // Parse expression until matching RParen
        ParserTokenSource source(*this);
        ExpressionParser expr(source);
        output = expr.parse_expression();
        return true;
    }

    // no arguments
    return false;
}

void Parser::skip_to_end_of_line() {
    while (current_.type != TokenType::EndOfLine &&
            current_.type != TokenType::EndOfInput) {
        advance(); // skip to end of line
    }
}

void Parser::parse_left_brace() {
    BraceFrame frame;
    frame.loc = current_.loc;
    frame.tape_ptr_at_start = output_.tape_ptr();
    brace_stack_.push_back(std::move(frame));
    advance(); // consume '{'
}

void Parser::parse_right_brace() {
    if (brace_stack_.empty()) {
        g_error_reporter.report_error(
            current_.loc,
            "unmatched '}' brace"
        );
        advance(); // consume '}'
        return;
    }
    int move_dist = brace_stack_.back().tape_ptr_at_start - output_.tape_ptr();
    if (move_dist != 0) {
        output_count_bf_instr(Token::make_bf('>', current_.loc), move_dist);
    }
    brace_stack_.pop_back();
    advance(); // consume '}'
}

bool Parser::if_branch_active() const {
    for (const auto& state : if_stack_) {
        if (!state.condition_true) {
            return false;
        }
    }
    return true;
}
