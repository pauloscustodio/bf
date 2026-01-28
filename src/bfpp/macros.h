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

bool is_reserved_keyword(const std::string& name);
