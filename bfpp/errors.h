//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "files.h"
#include <iostream>
#include <string>

class ErrorReporter {
public:
    void report_error(const SourceLocation& loc, const std::string& message);
    void report_warning(const SourceLocation& loc, const std::string& message);
    void report_note(const SourceLocation& loc, const std::string& message);

    int error_count() const;
    bool has_errors() const;
    void reset();

private:
    void report(const SourceLocation& loc, const std::string& message,
                const char* kind, bool increment_error);
    int error_count_ = 0;
};

extern ErrorReporter g_error_reporter;
