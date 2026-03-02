//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include "errors.h"

struct Symbol {
    std::string name;       // uppercase BASIC name, possibly with final $
    SourceLoc loc;
    bool is_array = false;
    bool is_string = false;
    int array_size = 0;     // only valid if is_array == true
    bool allocated = false;
    // if variable is assigned only once, it is a constant
    // and can be optimized as such
    int count_assignments = 0;
};

class SymbolTable {
public:
    // returns true if symbol already existed
    void declare_variable(const SourceLoc& loc, const std::string& name);
    void declare_array(const SourceLoc& loc, const std::string& name, int size);
    bool exists(const std::string& name) const;
    void mark_allocated(const std::string& name);
    bool is_allocated(const std::string& name) const;
    const std::unordered_map<std::string, Symbol>& all() const;
    Symbol& get(const std::string& name);
    const Symbol& get(const std::string& name) const;

private:
    std::unordered_map<std::string, Symbol> table;
};
