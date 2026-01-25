//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "output.h"
#include "errors.h"

void BFOutput::put(const Token& tok) {
    if (tok.type != TokenType::BFInstr) {
        g_error_reporter.report_error(
            tok.loc,
            "non-BF instruction token in output: '" + tok.text + "'"
        );
        return;
    }

    if (tok.text == ">") {
        tape_ptr_++;
    }
    else if (tok.text == "<") {
        if (tape_ptr_ == 0) {
            g_error_reporter.report_error(
                tok.loc,
                "tape pointer moved to negative position"
            );
            return;
        }
        tape_ptr_--;
    }
    else if (tok.text == "[") {
        loop_stack_.push_back(tok.loc);
    }
    else if (tok.text == "]") {
        if (loop_stack_.empty()) {
            g_error_reporter.report_error(
                tok.loc,
                "unmatched ']' instruction"
            );
            return;
        }
        loop_stack_.pop_back();
    }

    output_.push_back(tok);
}

std::string BFOutput::to_string() const {
    std::string result;
    int line_num = 1;
    int indent_level = 0;
    bool at_line_start = true;

    for (const Token& t : output_) {
        // synchronize line numbers with SourceLocation
        while (line_num < t.loc.line_num) {
            result += '\n';
            line_num++;
            at_line_start = true;
        }

        if (t.text == "[") {
            // Output '[' with current indentation on its own line
            if (!at_line_start) {
                result += '\n';
            }
            result += std::string(indent_level * 2, ' ');
            result += "[\n";
            indent_level++;
            at_line_start = true;
            // Don't increment line_num for bracket lines
        }
        else if (t.text == "]") {
            // Decrease indent, then output ']' on its own line
            if (!at_line_start) {
                result += '\n';
            }
            indent_level--;
            result += std::string(indent_level * 2, ' ');
            result += "]\n";
            at_line_start = true;
            // Don't increment line_num for bracket lines
        }
        else {
            // Regular BF instruction with indentation only at line start
            if (at_line_start) {
                result += std::string(indent_level * 2, ' ');
            }
            result += t.text;
            at_line_start = false;
        }
    }
    if (!at_line_start) {
        result += '\n';
    }
    return result;
}

void BFOutput::check_loops() const {
    for (auto& it : loop_stack_) {
        g_error_reporter.report_error(
            it,
            "unmatched '[' instruction"
        );
    }
}

int BFOutput::tape_ptr() const {
    return tape_ptr_;
}
