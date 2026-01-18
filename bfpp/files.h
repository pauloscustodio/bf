//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct SourceLocation {
    std::string filename;
    int line = 1;
    int column = 0;

    SourceLocation() = default;
    SourceLocation(const std::string& file, int lin, int col)
        : filename(file), line(lin), column(col) {
    }
};

class InputFile {
public:
    explicit InputFile(const std::string& filename);

    int get_char();      // returns next char or EOF
    void unget_char();   // optional, but useful for lexing
    const SourceLocation& location() const;
    bool is_open() const;
    bool is_eof() const;

private:
    std::ifstream stream_;
    SourceLocation loc_;
};

class FileStack {
public:
    bool push_file(const std::string& filename);
    void pop_file();

    int get_char();      // unified input
    void unget_char();   // unified unget
    bool is_eof() const;
    const SourceLocation& location() const;

    bool empty() const;

private:
    std::vector<std::unique_ptr<InputFile>> stack_;
};

extern FileStack g_file_stack;
