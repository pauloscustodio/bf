//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <string>
#include <istream>
#include "lexer.h"
#include "parser.h"

class Preprocessor {
public:
    Preprocessor();

    // Run with current configuration
    bool run(std::string& output);

    // Input setup
    bool push_file(const std::string& filename);
    bool push_file(const std::string& filename, const SourceLocation& loc);
    void push_stream(std::istream& stream, const std::string& virtual_name);

    // Config / metrics
    void set_stack_base(int base);
    int heap_size() const;
    int max_stack_depth() const;

private:
    CommentStripper stripper_;
    Lexer lexer_;
    Parser parser_;
};
