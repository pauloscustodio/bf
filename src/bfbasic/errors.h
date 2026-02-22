//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <string>

class SourceLoc {
public:
    SourceLoc(int line = 0) : line_(line) {}
    int line() const {
        return line_;
    }

private:
    int line_ = 0;
};

[[noreturn]] void error(const std::string& msg);
[[noreturn]] void error_at(const SourceLoc& loc, const std::string& msg);
