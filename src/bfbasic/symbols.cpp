//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "symbols.h"
#include <iostream>

// returns true if symbol already existed
bool SymbolTable::declare(const std::string& name) {
    auto it = table.find(name);
    if (it != table.end()) {
        return true;
    }

    table[name] = Symbol{ name, false };
    return false;
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
