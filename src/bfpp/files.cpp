//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "files.h"
#include "lexer.h"

FileStack g_file_stack;

InputFile::InputFile(const std::string& filename, std::istream* stream, bool owns_stream)
    : filename_(filename), stream_(stream), owns_stream_(owns_stream), line_num_(1) {
}

InputFile::~InputFile() {
    if (owns_stream_) {
        delete stream_;
    }
}

InputFile::InputFile(InputFile&& other) noexcept
    : filename_(std::move(other.filename_)),
      stream_(other.stream_),
      owns_stream_(other.owns_stream_),
      line_num_(other.line_num_) {
    // Transfer ownership - prevent other from deleting the stream
    other.stream_ = nullptr;
    other.owns_stream_ = false;
}

InputFile& InputFile::operator=(InputFile&& other) noexcept {
    if (this != &other) {
        // Delete current owned stream
        if (owns_stream_) {
            delete stream_;
        }

        // Transfer ownership from other
        filename_ = std::move(other.filename_);
        stream_ = other.stream_;
        owns_stream_ = other.owns_stream_;
        line_num_ = other.line_num_;

        // Prevent other from deleting the stream
        other.stream_ = nullptr;
        other.owns_stream_ = false;
    }
    return *this;
}

bool InputFile::getline(std::string& line) {
    line.clear();
    bool got_any = false;

    while (true) {
        int ch = stream_->get();
        if (ch == EOF) {
            if (got_any) {
                ++line_num_;  // Count line even if no trailing newline
            }
            return got_any;
        }

        if (ch == '\r') {
            int next = stream_->peek();
            if (next == '\n') {
                stream_->get(); // consume '\n' in CRLF
            }
            ++line_num_;
            return true;
        }

        if (ch == '\n') {
            ++line_num_;
            return true;
        }

        got_any = true;
        line.push_back(static_cast<char>(ch));
    }
}

bool InputFile::is_eof() const {
    return stream_->eof();
}

const std::string& InputFile::filename() const {
    return filename_;
}

int InputFile::line_num() const {
    return line_num_;
}

FileStack::~FileStack() {
    while (!stack_.empty()) {
        pop_file();
    }
}

void FileStack::add_include_path(const std::string& path) {
    file_include_path_.push_back(path);
}

bool FileStack::push_file(const std::string& filename) {
    return push_file(filename, SourceLocation(filename, 0, 0));
}

bool FileStack::push_file(const std::string& filename, const SourceLocation& loc) {
    std::string resolved = resolve_include_path(filename);
    std::ifstream* f = new std::ifstream(resolved);
    if (!*f) {
        delete f;
        f = nullptr;
        g_error_reporter.report_error(
            loc,
            "cannot open file '" + resolved + "'"
        );
        return false;
    }
    else {
        stack_.emplace_back(resolved, f, /*owns_stream=*/true);
        return true;
    }
}

void FileStack::push_stream(std::istream& s, const std::string& virtual_name) {
    stack_.emplace_back(virtual_name, &s, /*owns_stream=*/false);
}

void FileStack::pop_file() {
    if (!stack_.empty()) {
        stack_.pop_back(); // InputFile destructor handles owned streams
    }
}

bool FileStack::is_eof() const {
    if (!stack_.empty()) {
        return stack_.back().is_eof();
    }
    else {
        return true;
    }
}

const std::string& FileStack::filename() const {
    if (stack_.empty()) {
        static const std::string empty;
        return empty;
    }
    else {
        return stack_.back().filename();
    }
}

int FileStack::line_num() const {
    if (stack_.empty()) {
        return 0;
    }
    else {
        return stack_.back().line_num();
    }
}

std::string FileStack::resolve_include_path(const std::string& filename) {
    auto exists = [](const std::string & path) -> bool {
        std::ifstream f(path);
        return f.good();
    };

    // 1) As provided (current directory or absolute path)
    if (exists(filename)) {
        return filename;
    }

    // 2) Search include paths
    for (const auto& dir : file_include_path_) {
        if (dir.empty()) {
            continue;
        }
        std::string candidate;
        char last = dir.back();
        if (last == '/' || last == '\\') {
            candidate = dir + filename;
        }
        else {
            candidate = dir + "/" + filename;
        }

        if (exists(candidate)) {
            return candidate;
        }
    }

    // 3) Not found: return original
    return filename;
}

bool FileStack::getline(std::string& line) {
    while (!stack_.empty()) {
        if (stack_.back().getline(line)) {
            return true;
        }
        // EOF on this file; pop and continue with previous
        pop_file();
    }
    return false;
}
