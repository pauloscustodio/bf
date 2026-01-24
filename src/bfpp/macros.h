//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "files.h"
#include "lexer.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Macro {
    std::string name;
    std::vector<std::string> params;
    std::vector<Token> body;
    SourceLocation loc;
};

class MacroTable {
public:
    MacroTable() = default;

    bool define(const Macro& macro);
    void undef(const std::string& name);
    const Macro* lookup(const std::string& name) const;

private:
    std::unordered_map<std::string, Macro> table_;
};

extern MacroTable g_macro_table;

class Parser; // forward declaration

class MacroExpander {
public:
    MacroExpander(MacroTable& table);

    // Returns true if token was expanded, false otherwise
    bool try_expand(Parser& parser, const Token& token);
    void remove_expanding(const std::string& name);

private:
    MacroTable& table_;
    std::unordered_set<std::string> expanding_; // recursion guard

    std::vector<std::vector<Token>> collect_args(Parser& parser, 
        const Macro& macro);
    std::vector<Token> substitute_body(const Macro& macro,
        const std::vector<std::vector<Token>>& args);
};

#if 0

class MacroExpander {
public:

    void expand_token(const Token& tok, std::deque<Token>& out);
    void expand_macro_body(const Macro& m,
                           const std::vector<Token>& substituted_body,
                           std::deque<Token>& out);

private:
    int pointer_pos_ = 0;

    bool try_expand_extended_bf(TokenStream& in,
                                std::deque<Token>& out);
    bool expand_extended_bf(const std::vector<Token>& body,
                            size_t& i,
                            std::deque<Token>& out);
    int eval_expr_tokens(const std::vector<Token>& expr,
                         const SourceLocation& loc);
    void emit_bf_token(const Token& t,
                       std::deque<Token>& out);
    void apply_extended_bf_op(const Token& op,
                              int value,
                              std::deque<Token>& out);
    std::vector<Token> resolve_ident_to_expr_tokens(const Token& ident);
};

bool contains_comment_tokens(const std::vector<Token>& body);
bool contains_directive_token(const std::vector<Token>& body);
bool is_reserved_keyword(const std::string& name);
bool token_allowed_in_expression(const Token& t);
bool all_tokens_allowed_in_expression(const std::vector<Token>& tokens);

#endif
