//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"

ErrorReporter g_error_reporter;

void ErrorReporter::report_error(const SourceLocation& loc, const std::string& message) {
    report(loc, message, "error", true);
}

void ErrorReporter::report_warning(const SourceLocation& loc, const std::string& message) {
    report(loc, message, "warning", false);
}

void ErrorReporter::report_note(const SourceLocation& loc, const std::string& message) {
    report(loc, message, "note", false);
}

void ErrorReporter::report(const SourceLocation& loc, const std::string& message,
                           const char* kind, bool increment_error) {
    std::cerr
        << loc.filename << ":"
        << loc.line << ":"
        << loc.column << ": "
        << kind << ": "
        << message
        << std::endl;

    if (increment_error) {
        error_count_++;
    }
}

int ErrorReporter::error_count() const {
    return error_count_;
}

bool ErrorReporter::has_errors() const {
    return error_count_ > 0;
}

void ErrorReporter::reset() { 
    error_count_ = 0; 
}
