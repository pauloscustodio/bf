//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "files.h"
#include "lexer.h"

FileStack g_file_stack;

InputFile::InputFile(const std::string& filename)
    : stream_(filename, std::ios::binary), loc_(filename, 1, 0) {
}

int InputFile::get_char() {
    int ch = stream_.get();
    if (ch == '\n') {
        loc_.line++;
        loc_.column = 0;
    }
    else if (ch != EOF) {
        loc_.column++;
    }
    return ch;
}

void InputFile::unget_char() {
    // Only valid if the last operation was get()
    stream_.unget();
    if (loc_.column > 0) {
        loc_.column--;
    }
    // If column == 0, we would need to track previous line lengths.
    // For bfpp, we simply forbid ungetting across line boundaries.
}

// optional, but useful for lexing
const SourceLocation& InputFile::location() const {
    return loc_;
}

bool InputFile::is_open() const {
    return stream_.is_open();
}

bool InputFile::is_eof() const { 
    return stream_.eof(); 
}

bool FileStack::push_file(const std::string& filename) {
    // detect include loops
    for (const auto& file : stack_) {
        if (file->location().filename == filename) {
            g_error_reporter.report_error(
                file->location(),
                "include loop detected for file '" + filename + "'"
            );
            return false;
        }
    }

    // open the file
    auto new_file = std::make_unique<InputFile>(filename);
    if (!new_file->is_open()) {
        g_error_reporter.report_error(
            SourceLocation{ filename, 1, 0 },
            "could not open file"
        );
        return false;
    }

    stack_.push_back(std::move(new_file));
    return true;
}

void FileStack::pop_file() {
    if (!stack_.empty()) {
        stack_.pop_back();
    }
}

int FileStack::get_char() {
    while (!stack_.empty()) {
        int ch = stack_.back()->get_char();
        if (ch != EOF) {
            return ch;
        }
        pop_file();
    }
    return EOF;
}

void FileStack::unget_char() {
    if (!stack_.empty()) {
        stack_.back()->unget_char();
    }
}

bool FileStack::is_eof() const {
    if (!stack_.empty()) {
        stack_.back()->is_eof();
    }

    return true;
}

const SourceLocation& FileStack::location() const {
    if (stack_.empty()) {
        static SourceLocation unknown_loc("<<no file>>", 0, 0);
        return unknown_loc;
    }

    return stack_.back()->location();
}

bool FileStack::empty() const {
    return stack_.empty();
}
