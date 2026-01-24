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

class Parser {
public:
    Parser(Lexer& lexer);

    bool run(std::string& output);
    const Token& current() const {
        return current_;
    }
    Token peek(size_t offset = 0);
    void advance();

private:
    Lexer lexer_;
    std::vector<MacroExpansionFrame> expansion_stack_;
    MacroExpander macro_expander_;
    Token current_;
    BFOutput output_;

    friend class MacroExpander;

    bool parse();
    std::string to_string() const;

    void parse_directive();
    void parse_statements();
    void parse_statement();
    void parse_bfinstr();
    int parse_repeat_count();
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
    bool check_macro_body_token_boundaries(const std::string& name,
                                           const SourceLocation& name_loc,
                                           const std::vector<Token>& body);
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
