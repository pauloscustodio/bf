//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>

struct Symbol {
    std::string name;   // uppercase BASIC name
    bool allocated = false;
};

class SymbolTable {
public:
    // returns true if symbol already existed
    bool declare(const std::string& name);
    bool exists(const std::string& name) const;
    void mark_allocated(const std::string& name);
    bool is_allocated(const std::string& name) const;
    const std::unordered_map<std::string, Symbol>& all() const;
    Symbol& get(const std::string& name);
    const Symbol& get(const std::string& name) const;

private:
    std::unordered_map<std::string, Symbol> table;
};
