//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include <iostream>

[[noreturn]]
void error(const std::string& msg) {
    std::cerr << "Error: " << msg << std::endl;
    exit(EXIT_FAILURE);
}

[[noreturn]]
void error_at(const SourceLoc& loc, const std::string& msg) {
    if (loc.line() == 0) {
        error(msg);
    }
    else {
        std::cerr << "Error at line " << loc.line() << ": " << msg << std::endl;
    }
    exit(EXIT_FAILURE);
}
