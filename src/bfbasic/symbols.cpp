//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "symbols.h"
#include <iostream>

// returns true if symbol already existed
void SymbolTable::declare_variable(const SourceLoc& loc, const std::string& name) {
    auto it = table.find(name);
    if (it != table.end()) {
        if (it->second.is_array) {
            error_at(loc, "Variable '" + name + "' already declared as array");
        }
    }
    else {
        Symbol s;
        s.name = name;
        s.loc = loc;
        s.is_array = false;
        table[name] = std::move(s);
    }
}

void SymbolTable::declare_array(const SourceLoc& loc, const std::string& name, int size) {
    auto it = table.find(name);
    if (it != table.end()) {
        if (!it->second.is_array) {
            error_at(loc, "Variable '" + name + "' already declared as simple variable");
        }
    }
    else {
        auto& s = table[name];
        s.name = name;
        s.loc = loc;
        s.is_array = true;
        s.array_size = size;
    }
}

bool SymbolTable::exists(const std::string& name) const {
    return table.count(name) != 0;
}

void SymbolTable::mark_allocated(const std::string& name) {
    table[name].allocated = true;
}

bool SymbolTable::is_allocated(const std::string& name) const {
    auto it = table.find(name);
    return it != table.end() && it->second.allocated;
}

const std::unordered_map<std::string, Symbol>& SymbolTable::all() const {
    return table;
}

Symbol& SymbolTable::get(const std::string& name) {
    auto it = table.find(name);
    if (it == table.end()) {
        std::cerr << "Internal error: unknown symbol '" << name << "'\n";
        std::exit(EXIT_FAILURE);
    }
    return it->second;
}

const Symbol& SymbolTable::get(const std::string& name) const {
    auto it = table.find(name);
    if (it == table.end()) {
        std::cerr << "Internal error: unknown symbol '" << name << "'\n";
        std::exit(EXIT_FAILURE);
    }
    return it->second;
}
