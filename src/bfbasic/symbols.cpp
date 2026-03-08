//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "lexer.h"
#include "symbols.h"
#include <iostream>

void SymbolTable::clear() {
    table.clear();
}

void SymbolTable::declare(const std::string& name, SymbolType type,
                          const SourceLoc& loc) {
    auto it = table.find(name);
    if (it != table.end()) {    // already declared
        if (it->second.type != type) {
            error_at(loc,
                     "Variable '" + name + "' already declared with a different type");
        }
    }
    else {                      // not declared yet
        auto& s = table[name];
        s.name = name;
        s.type = type;
        s.loc = loc;
    }
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

Symbol* SymbolTable::get(const std::string& name) {
    auto it = table.find(name);
    if (it == table.end()) {
        return nullptr;
    }
    else {
        return &it->second;
    }
}

const Symbol* SymbolTable::get(const std::string& name) const {
    auto it = table.find(name);
    if (it == table.end()) {
        return nullptr;
    }
    else {
        return &it->second;
    }
}

