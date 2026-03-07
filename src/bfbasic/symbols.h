//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include "errors.h"

enum class SymbolType {
    IntVar,
    IntArrayVar,
    StringVar
};

struct Symbol {
    std::string name;       // uppercase BASIC name, possibly with final $
    SymbolType type = SymbolType::IntVar;
    SourceLoc loc;

    int array_size = 0;     // for IntArrayVar and StringVar, 0 for IntVar
    bool allocated = false; // allocation code already output

    // if variable is assigned only once, it is a constant
    // and can be optimized as such
    int count_assignments = 0;
    // set when count_assignments == 1 and RHS is a folded number
    std::optional<int> const_value;
    // set when count_assignments == 1 and RHS is a folded string
    std::optional<std::string> const_string;
};

class SymbolTable {
public:
    void declare(const std::string& name, SymbolType type,
                 const SourceLoc& loc);
    void mark_allocated(const std::string& name);
    bool is_allocated(const std::string& name) const;
    const std::unordered_map<std::string, Symbol>& all() const;

    // return nullptr if not found
    Symbol* get(const std::string& name);
    const Symbol* get(const std::string& name) const;

private:
    std::unordered_map<std::string, Symbol> table;
};
