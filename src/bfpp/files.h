//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <fstream>
#include <string>
#include <vector>

struct SourceLocation {
    std::string filename;
    int line_num = 0;
    int column = 0;

    SourceLocation() = default;
    SourceLocation(const std::string& file, int lin, int col)
        : filename(file), line_num(lin), column(col) {
    }
};

class InputFile {
public:
    InputFile(const std::string& filename, std::istream* stream, bool owns_stream);
    virtual ~InputFile();

    bool getline(std::string& line);
    bool is_eof() const;
    const std::string& filename() const;
    int line_num() const;

private:
    std::string filename_;
    std::istream* stream_ = nullptr;
    bool owns_stream_ = false;
    int line_num_ = 1;
};

class FileStack {
public:
    FileStack() = default;
    virtual ~FileStack();

    bool push_file(const std::string& filename);
    bool push_file(const std::string& filename, const SourceLocation& loc);
    void push_stream(std::istream& s, const std::string& virtual_name);
    void pop_file();

    bool getline(std::string& line);
    bool is_eof() const;
    const std::string& filename() const;
    int line_num() const;

private:
    std::vector<InputFile> stack_;
};

extern FileStack g_file_stack;
