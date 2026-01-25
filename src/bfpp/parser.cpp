//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "expr.h"
#include "macros.h"
#include "parser.h"

Parser::Parser(Lexer& lexer)
    : lexer_(lexer), macro_expander_(g_macro_table) {
}

bool Parser::run(std::string& output_) {
    advance();  // get first token
    if (parse()) {
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

std::string Parser::to_string() const {
    return output_.to_string();
}

bool Parser::parse() {
    while (true) {
        if (current_.type == TokenType::EndOfInput) {
            break;
        }

        if (current_.type == TokenType::EndOfLine) {
            advance();
            continue;
        }

        if (current_.type == TokenType::Error) {
            return false;
        }

        if (current_.type == TokenType::Directive) {
            parse_directive();
        }
        else {
            parse_statements();
        }
    }

    // After finishing, check for unmatched loops
    output_.check_loops();

    return !g_error_reporter.has_errors();
}

void Parser::parse_directive() {
    Token directive = current_;
    advance(); // consume directive keyword

    if (directive.text == "#include") {
        parse_include();
    }
    else {
        g_error_reporter.report_error(
            directive.loc,
            "unknown directive: '" + directive.text + "'"
        );
        skip_to_end_of_line();
    }

    if (current_.type != TokenType::EndOfLine &&
            current_.type != TokenType::EndOfInput) {
        g_error_reporter.report_error(
            current_.loc,
            "unexpected token after " + directive.text +
            ": '" + current_.text + "'"
        );
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

void Parser::parse_statements() {
    while (current_.type != TokenType::EndOfLine &&
            current_.type != TokenType::EndOfInput &&
            current_.type != TokenType::Error) {
        parse_statement();
    }
}

void Parser::parse_statement() {
    // Try macro expansion first
    while (macro_expander_.try_expand(*this, current_)) {
        advance(); // load first token from expansion
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
    int arg = 0;
    if (parse_bf_int_arg(arg)) {
        int delta = arg - pos;
        output_count_bf_instr(Token::make_bf('>', tok.loc), delta);
        pos = arg;
    }
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

    if (current_.type == TokenType::Identifier) {
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

#if 0

std::string Parser::run() {
    while (current_.type != TokenType::EndOfFile &&
            current_.type != TokenType::Error) {

        parse_statements();
    }

    // After finishing the file, check for unclosed #if blocks
    if (!if_stack_.empty()) {
        const IfState& state = if_stack_.back();
        g_error_reporter.report_error(
            state.loc,  // store loc when pushing IfState
            "unterminated #if (missing #endif)"
        );
    }

    return to_string();
}


Token Parser::current() const {
    return current_;
}

void Parser::advance() {
    // If we already have expanded tokens, consume one
    if (!expanded_.empty()) {
        current_ = expanded_.front();
        expanded_.pop_front();
        return;
    }

    while (expanded_.empty()) {
        Token raw = lexer_.get();

        if (raw.type == TokenType::EndOfFile) {
            current_ = raw;
            return;
        }

        if (raw.type == TokenType::Identifier) {
            const Macro* m = g_macro_table.lookup(raw.text);
            if (m) {
                // Peek: function-like or object-like?
                Token next = lexer_.peek();
                if (!m->params.empty() && next.type == TokenType::LParen) {
                    parse_macro_invocation(*m, raw.loc);
                    continue; // expanded_ now has tokens
                }
                else if (m->params.empty()) {
                    // object-like: just substitute body
                    substitute_object_like_macro(*m);
                    continue;
                }
            }
        }

        // Not a macro, or not a call: expand normally (hygiene etc.)
        macro_expander_.expand_token(raw, expanded_);
    }

    current_ = expanded_.front();
    expanded_.pop_front();
}

void Parser::parse_macro_invocation(const Macro& m, SourceLocation call_loc) {
    // We already consumed the identifier in advance(), and peeked '('.
    Token lparen = lexer_.get(); // consume '('
    (void)lparen; // we know it's '('

    std::vector<std::vector<Token>> args;

    // Empty arg list?
    Token t = lexer_.peek();
    if (t.type != TokenType::RParen) {
        while (true) {
            std::vector<Token> expr = parse_macro_argument_expression();
            args.push_back(std::move(expr));

            t = lexer_.peek();
            if (t.type == TokenType::RParen) {
                break;
            }

            if (t.type != TokenType::Comma) {
                g_error_reporter.report_error(
                    t.loc,
                    "expected ',' or ')' in macro call"
                );
                // Try to resync: consume until ')'
                do {
                    t = lexer_.get();
                }
                while (t.type != TokenType::RParen &&
                        t.type != TokenType::EndOfFile);
                break;
            }

            lexer_.get(); // consume ','
        }
    }

    Token rparen = lexer_.get(); // consume ')'
    (void)rparen;

    // Arity check
    if (args.size() != m.params.size()) {
        g_error_reporter.report_error(
            call_loc,
            "macro '" + m.name + "' expects " +
            std::to_string(m.params.size()) +
            " arguments but got " +
            std::to_string(args.size())
        );
        return;
    }

    substitute_function_like_macro(m, args);
}

std::vector<Token> Parser::parse_macro_argument_expression() {
    std::vector<Token> result;
    int paren_depth = 0;

    while (true) {
        Token t = lexer_.get();

        if (t.type == TokenType::EndOfFile) {
            break;
        }

        if (t.type == TokenType::LParen) {
            paren_depth++;
            result.push_back(t);
            continue;
        }

        if (t.type == TokenType::RParen) {
            if (paren_depth == 0) {
                // Put it back for caller (we stop before ')')
                lexer_.unget(t);
                break;
            }
            paren_depth--;
            result.push_back(t);
            continue;
        }

        if (t.type == TokenType::Comma && paren_depth == 0) {
            // Stop before comma; caller will consume it
            lexer_.unget(t);
            break;
        }

        result.push_back(t);
    }

    return result;
}

void Parser::substitute_object_like_macro(const Macro& m) {
    for (const Token& t : m.body) {
        expanded_.push_back(t);
    }
}

void Parser::substitute_function_like_macro(const Macro& m, const std::vector<std::vector<Token>>& args) {
    std::unordered_map<std::string, const std::vector<Token>*> param_map;

    for (size_t i = 0; i < m.params.size(); ++i) {
        param_map[m.params[i]] = &args[i];
    }

    for (const Token& t : m.body) {
        if (t.type == TokenType::Identifier) {
            auto it = param_map.find(t.text);
            if (it != param_map.end()) {
                // Insert argument tokens
                for (const Token& a : *it->second) {
                    expanded_.push_back(a);
                }
                continue;
            }
        }
        expanded_.push_back(t);
    }
}

Token Parser::next_raw_token() {
    while (!expansion_stack_.empty()) {
        auto& frame = expansion_stack_.back();
        if (frame.index < frame.tokens.size()) {
            return frame.tokens[frame.index++];
        }
        expansion_stack_.pop_back();
    }
    return lexer_.get();
}

bool Parser::match(TokenType type) {
    if (current_.type == type) {
        advance();
        return true;
    }
    return false;
}

void Parser::expect(TokenType type, const std::string& message) {
    if (current_.type != type) {
        g_error_reporter.report_error(current_.loc, message);
        return;
    }
    advance();
}

void Parser::parse_directive() {
    Token directive = current_;
    set_expansion_context(ExpansionContext::DirectiveKeyword);
    advance(); // consume directive keyword
    set_expansion_context(ExpansionContext::Normal);

    if (directive.text == "#include") {
        parse_include();
        return;
    }

    if (directive.text == "#define") {
        parse_define();
        return;
    }

    if (directive.text == "#undef") {
        parse_undef();
        return;
    }

    if (directive.text == "#if") {
        parse_if();
        return;
    }

    if (directive.text == "#elsif") {
        parse_elsif();
        return;
    }

    if (directive.text == "#else") {
        parse_else();
        return;
    }

    if (directive.text == "#endif") {
        parse_endif();
        return;
    }

    g_error_reporter.report_error(
        directive.loc,
        "unknown directive '" + directive.text + "'"
    );
    advance(); // consume unknown directive to avoid infinite loop
}



void Parser::parse_define() {
    // At entry: current_ is the identifier after '#define'
    SourceLocation define_loc = current_.loc;

    // --- 1. Expect macro name ---
    if (current_.type != TokenType::Identifier) {
        g_error_reporter.report_error(current_.loc, "expected macro name");
        skip_to_end_of_line(define_loc.line);
        return;
    }

    std::string name = current_.text;
    SourceLocation name_loc = current_.loc;
    advance(); // consume name

    // Reserved keyword check (hygiene rule)
    if (is_reserved_keyword(name)) {
        g_error_reporter.report_error(
            name_loc,
            "cannot define macro '" + name + "': reserved directive keyword"
        );
        skip_to_end_of_line(define_loc.line);
        return;
    }

    std::vector<std::string> params;
    std::vector<Token> body;


    // --- 2. Function-like macro? ---
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
                    skip_to_end_of_line(define_loc.line);
                    return;
                }

                params.push_back(current_.text);
                advance(); // consume parameter

                if (current_.type == TokenType::RParen) {
                    break;
                }

                if (current_.type != TokenType::Comma) {
                    g_error_reporter.report_error(
                        current_.loc,
                        "expected ',' or ')'"
                    );
                    skip_to_end_of_line(define_loc.line);
                    return;
                }

                advance(); // consume comma
            }
        }

        advance(); // consume ')'

        // --- 3. Multi-line body until #end ---
        while (true) {
            advance();

            if (current_.type == TokenType::EndOfFile) {
                g_error_reporter.report_error(
                    name_loc,
                    "unterminated macro '" + name + "': missing #end"
                );
                return;
            }

            if (current_.type == TokenType::Directive &&
                    current_.text == "#end") {
                break;
            }

            body.push_back(current_);
        }
        // consume the '#end'
        advance();
    }
    else {
        // --- 4. Object-like macro ---
        // Two cases:
        //   a) single-line: #define X 3
        //   b) multi-line:  #define X ... #end

        bool single_line = (current_.loc.line == name_loc.line);

        if (single_line) {
            // Collect tokens until end of line
            while (current_.type != TokenType::EndOfFile &&
                    current_.loc.line == name_loc.line) {
                body.push_back(current_);
                advance();
            }
        }
        else {
            // Multi-line object-like macro
            while (true) {
                advance();

                if (current_.type == TokenType::EndOfFile) {
                    g_error_reporter.report_error(
                        name_loc,
                        "unterminated macro '" + name + "': missing #end"
                    );
                    return;
                }

                if (current_.type == TokenType::Directive &&
                        current_.text == "#end") {
                    break;
                }

                body.push_back(current_);
            }
            // consume the '#end'
            advance();
        }
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

    // --- 5. Hygiene checks ---
    if (!check_macro_body_token_boundaries(name, name_loc, body)) {
        return;
    }

    if (contains_directive_token(body)) {
        g_error_reporter.report_error(
            name_loc,
            "macro '" + name + "' contains a directive, which is not allowed"
        );
        return;
    }

    if (contains_comment_tokens(body)) {
        g_error_reporter.report_error(
            name_loc,
            "macro '" + name + "' contains comment delimiters, which is not allowed"
        );
        return;
    }

    // --- 6. Store macro ---
    Macro macro;
    macro.name = name;
    macro.loc = name_loc;
    macro.params = params;
    macro.body = body;
    g_macro_table.define(macro);
}

void Parser::skip_to_end_of_line(int line) {
    while (current_.type != TokenType::EndOfFile &&
            current_.type != TokenType::EndOfExpression &&
            current_.loc.line == line) {
        advance();
    }
}

void Parser::parse_undef() {
    advance(); // consume #undef

    if (current_.type != TokenType::Identifier) {
        g_error_reporter.report_error(current_.loc, "expected macro name");
        return;
    }

    std::string name = current_.text;

    if (is_reserved_keyword(lowercase(name))) {
        g_error_reporter.report_error(
            current_.loc,
            "cannot undefine reserved directive keyword '" + name + "'"
        );
        advance();
        return;
    }

    g_macro_table.undef(name);
    advance();
}

bool Parser::check_macro_body_token_boundaries(const std::string& name,
        const SourceLocation& name_loc,
        const std::vector<Token>& body) {
    if (body.empty()) {
        return true;    // empty macro is always safe
    }

    const Token& first = body.front();
    const Token& last = body.back();

    auto is_glue_sensitive = [](const Token & t) -> bool {
        if (t.type == TokenType::Identifier) {
            return true;
        }
        if (t.type == TokenType::Integer) {
            return true;
        }

        if (t.type == TokenType::Operator) {
            const std::string& op = t.text;
            if (op == "<" || op == ">" || op == "=" || op == "!" ||
                    op == "&" || op == "|" || op == "+" || op == "-" ||
                    op == "/" || op == "%" || op == "^") {
                return true;
            }
        }

        if (t.type == TokenType::Directive) {
            // body starting with '#' is not allowed (Rule 5 will also cover this)
            if (!t.text.empty() && t.text[0] == '#') {
                return true;
            }
        }

        return false;
    };

    if (is_glue_sensitive(first) || is_glue_sensitive(last)) {
        g_error_reporter.report_error(
            name_loc,
            "macro '" + name + "' has a body that may break token boundaries"
        );
        return false;
    }

    return true;
}

void Parser::parse_if() {
    SourceLocation loc = current_.loc;   // location of #if

    // Switch lexer to expression mode
    lexer_.set_mode(ExpansionContext::DirectiveExpression);
    set_expansion_context(ExpansionContext::DirectiveExpression);

    ExpressionParser expr(*this);
    int value = expr.parse_expression();

    // Restore normal mode
    lexer_.set_mode(ExpansionContext::Normal);
    set_expansion_context(ExpansionContext::Normal);

    IfState state;
    state.condition_true = (value != 0);
    state.branch_taken = state.condition_true;
    state.in_else = false;
    state.loc = loc;

    if_stack_.push_back(state);

    // If false, skip until next branch
    if (!state.condition_true) {
        skip_until_else_or_endif();
    }
}

void Parser::parse_elsif() {
    SourceLocation loc = current_.loc;   // location of #elsif

    if (if_stack_.empty()) {
        g_error_reporter.report_error(loc, "#elsif without matching #if");
        return;
    }

    IfState& state = if_stack_.back();

    if (state.in_else) {
        g_error_reporter.report_error(loc, "#elsif after #else");
        return;
    }

    // If a previous branch was already taken, skip this one
    if (state.branch_taken) {
        skip_until_else_or_endif();
        return;
    }

    // Evaluate the new condition
    lexer_.set_mode(ExpansionContext::DirectiveExpression);
    set_expansion_context(ExpansionContext::DirectiveExpression);

    ExpressionParser expr(*this);
    int value = expr.parse_expression();

    lexer_.set_mode(ExpansionContext::Normal);
    set_expansion_context(ExpansionContext::Normal);

    state.condition_true = (value != 0);

    if (state.condition_true) {
        state.branch_taken = true;
    }
    else {
        skip_until_else_or_endif();
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

    // If no branch has been taken yet, this one executes
    if (!state.branch_taken) {
        state.condition_true = true;
        state.branch_taken = true;
    }
    else {
        // Otherwise skip until #endif
        skip_until_endif();
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

bool Parser::read_directive_keyword(std::string& out) {
    if (current_.type != TokenType::Directive) {
        return false;
    }

    out = current_.text;   // "#if", "#else", "#endif", "#elsif", etc.
    return true;
}

void Parser::skip_until_else_or_endif() {
    int depth = 0;

    while (true) {
        advance();  // consume current token

        if (current_.type == TokenType::EndOfFile) {
            return;
        }

        // Only directives matter during skipping
        if (current_.type != TokenType::Directive) {
            continue;
        }

        std::string kw = current_.text;

        if (kw == "#if") {
            depth++;
            continue;
        }

        if (kw == "#endif") {
            if (depth == 0) {
                // End of the current conditional block
                return;
            }
            depth--;
            continue;
        }

        if (depth == 0) {
            // Only consider #else and #elsif at depth 0
            if (kw == "#else" || kw == "#elsif") {
                return;
            }
        }

        // Otherwise ignore directive and continue
    }
}

void Parser::skip_until_endif() {
    int depth = 0;

    while (true) {
        advance();

        if (current_.type == TokenType::EndOfFile) {
            return;
        }

        if (current_.type != TokenType::Directive) {
            continue;
        }

        std::string kw = current_.text;

        if (kw == "#if") {
            depth++;
            continue;
        }

        if (kw == "#endif") {
            if (depth == 0) {
                // End of the current #if block
                return;
            }
            depth--;
            continue;
        }

        // #else and #elsif are ignored here because we are skipping
        // the entire remainder of the block after #else.
    }
}

void Parser::parse_statements() {
    // Expand macros until no more expansions occur
    while (try_expand_macro()) {
        // keep expanding
    }

    switch (current_.type) {
    case TokenType::BFInstr:
        emit_bf_instruction(current_);
        advance();
        return;

    case TokenType::Identifier:
        g_error_reporter.report_error(
            current_.loc,
            "unknown identifier '" + current_.text + "'"
        );
        advance();
        return;

    default:
        g_error_reporter.report_error(
            current_.loc,
            "unexpected token in statement: '" + current_.text + "'"
        );
        advance();
        return;
    }
}






void Parser::emit_bf_instruction(const Token& t) {
    // All BF instructions must go through the macro expander
    macro_expander_.expand_token(t, expanded_);
}


ExpansionContext Parser::expansion_context() const {
    return context_;
}

void Parser::set_expansion_context(ExpansionContext c) {
    context_ = c;
}

#endif
