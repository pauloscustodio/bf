//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "preprocessor.h"
#include "files.h"

Preprocessor::Preprocessor() :
    lexer_(stripper_), parser_(lexer_) {}

// Entry point: run the preprocessing pipeline and return the output.
bool Preprocessor::run(std::string& output) {
    return parser_.run(output); 
}

bool Preprocessor::push_file(const std::string& filename) {
    return g_file_stack.push_file(filename);
}

bool Preprocessor::push_file(const std::string& filename, const SourceLocation& loc) {
    return g_file_stack.push_file(filename, loc);
}

void Preprocessor::push_stream(std::istream& stream, const std::string& virtual_name) {
    g_file_stack.push_stream(stream, virtual_name);
}
