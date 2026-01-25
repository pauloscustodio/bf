//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "files.h"
#include "lexer.h"
#include <string>
#include <vector>

class BFOutput {
public:
    BFOutput() = default;

    void put(const Token& tok);
    std::string to_string() const;
    void check_loops() const;
    int tape_ptr() const;

private:
    int tape_ptr_ = 0;
    std::vector<SourceLocation> loop_stack_;
    std::vector<Token> output_;
};
