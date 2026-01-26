//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "lexer.h"
#include "macros.h"
#include <vector>
#include <string>
#include "output.h"

struct MacroExpansionFrame {
    std::string macro_name;
    std::vector<Token> tokens;
    std::size_t index = 0;
};

struct LoopFrame {
    SourceLocation loc;
    int tape_ptr_at_start = 0;
};

struct BraceFrame {
    SourceLocation loc;
    int tape_ptr_at_start = 0;
};

class Parser {
public:
    Parser(Lexer& lexer);

    bool run(std::string& output);
    const Token& current() const;
    Token peek(size_t offset = 0);
    void advance();

private:
    Lexer lexer_;
    std::vector<MacroExpansionFrame> expansion_stack_;
    std::vector<LoopFrame> loop_stack_;
    std::vector<BraceFrame> brace_stack_;
    MacroExpander macro_expander_;
    Token current_;
    BFOutput output_;

    friend class MacroExpander;

    std::string to_string() const;

    bool parse();
    void parse_directive();
    void parse_include();
    void parse_define();
    void parse_statements();
    void parse_statement();
    void parse_bfinstr();
    void parse_bf_plus_minus(const Token& tok);
    void parse_bf_left_right(const Token& tok);
    void parse_bf_loop_start(const Token& tok);
    void parse_bf_loop_end(const Token& tok);
    void parse_bf_input(const Token& tok);
    void parse_bf_output(const Token& tok);
    void output_count_bf_instr(const Token& tok, int count);
    bool parse_bf_int_arg(int& output);
    void skip_to_end_of_line();
    void parse_left_brace();
    void parse_right_brace();
};


#if 0
struct IfState {
    bool condition_true;   // result of the current branch
    bool branch_taken;     // whether ANY branch has been taken
    bool in_else;          // whether #else has been seen
    SourceLocation loc;    // location of the #if
};

class Parser {
public:
    Parser(Lexer& lexer);

    std::string run();
    std::string to_string() const;
    Token current() const;
    void advance();

private:
    Lexer lexer_;
    MacroExpander macro_expander_;
    Token current_;
    std::deque<Token> expanded_;
    std::unordered_set<std::string> expanding_macros_;
    std::vector<IfState> if_stack_;
    ExpansionContext context_ = ExpansionContext::Normal;

    Token next_raw_token(); // from stack or lexer

    bool match(TokenType type);
    void expect(TokenType type, const std::string& message);
    void parse_include();
    void parse_define();
    void parse_undef();
    void skip_to_end_of_line(int line);
    void parse_if();
    void parse_elsif();
    void parse_else();
    void parse_endif();
    bool read_directive_keyword(std::string& out);
    void skip_until_else_or_endif();
    void skip_until_endif();
    void parse_statements();
    void parse_macro_invocation(const Macro& m, SourceLocation call_loc);
    std::vector<Token> parse_macro_argument_expression();
    void substitute_object_like_macro(const Macro& m);
    void substitute_function_like_macro(const Macro& m, const std::vector<std::vector<Token>>& args);
    void emit_bf_instruction(const Token& token);
    ExpansionContext expansion_context() const;
    void set_expansion_context(ExpansionContext c);
};
#endif
