//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "files.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: bfpp <source-file>" << std::endl;
        return EXIT_FAILURE;
    }

    if (g_file_stack.push_file(argv[1])) {
        g_error_reporter.report_error(
            SourceLocation{ argv[1], 1, 0 },
            "could not open file"
        );
        return EXIT_FAILURE;
    }

    // run lexer, parser, expander, emitter...

    return g_error_reporter.has_errors()
        ? EXIT_FAILURE
        : EXIT_SUCCESS;
}
