//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "preprocessor.h"
#include "macros.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    std::string input_filename;
    std::string output_filename;

    // --- Parse command-line arguments ---
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "bfpp: missing filename after -o" << std::endl;
                return EXIT_FAILURE;
            }
            output_filename = argv[++i];
            continue;
        }

        // Handle -D name=value, -Dname=value, or -D name (defaults to 1)
        if (arg == "-D" || (arg.length() > 2 && arg.substr(0, 2) == "-D")) {
            std::string def;
            if (arg == "-D") {
                if (i + 1 >= argc) {
                    std::cerr << "bfpp: missing argument after -D" << std::endl;
                    return EXIT_FAILURE;
                }
                def = argv[++i];
            }
            else {
                def = arg.substr(2);  // -Dname=value or -Dname
            }

            // Parse name=value or just name (defaults to 1)
            size_t eq_pos = def.find('=');
            std::string name = (eq_pos != std::string::npos) ? def.substr(0, eq_pos) : def;
            std::string value_str = (eq_pos != std::string::npos) ? def.substr(eq_pos + 1) : "1";

            // Validate name is a valid identifier
            if (name.empty() || (!std::isalpha(name[0]) && name[0] != '_')) {
                std::cerr << "bfpp: invalid macro name: " << name << std::endl;
                return EXIT_FAILURE;
            }

            // Validate remaining characters of name
            for (char c : name) {
                if (!std::isalnum(c) && c != '_') {
                    std::cerr << "bfpp: invalid macro name: " << name << std::endl;
                    return EXIT_FAILURE;
                }
            }

            // Parse integer value
            try {
                int value = std::stoi(value_str);

                // Create macro with single integer token
                Macro macro;
                macro.name = name;
                macro.params = {};  // object-like macro
                macro.body.push_back(Token::make_int(value,
                    SourceLocation("<command-line>", 0, 0)));
                macro.loc = SourceLocation("<command-line>", 0, 0);

                g_macro_table.define(macro);
            }
            catch (const std::exception&) {
                std::cerr << "bfpp: invalid integer value: " << value_str << std::endl;
                return EXIT_FAILURE;
            }
            continue;
        }

        if (arg[0] == '-') {
            std::cerr << "error: Usage: bfpp [-o output_file] [-D name=value] [input_file]" << std::endl;
            return EXIT_FAILURE;
        }

        if (!input_filename.empty()) {
            std::cerr << "bfpp: only one input file may be specified" << std::endl;
            return 1;
        }

        input_filename = arg;
    }

    // Open output stream
    std::ostream* out = &std::cout;
    std::ofstream outfile;

    if (!output_filename.empty()) {
        outfile.open(output_filename);
        if (!outfile) {
            std::cerr << "bfpp: cannot open output file: "
                      << output_filename << "\n";
            return 1;
        }
        out = &outfile;
    }

    Preprocessor pp;

    // Open input stream
    if (input_filename.empty()) {
        // Read from stdin
        pp.push_stream(std::cin, "<stdin>");
    }
    else {
        if (!pp.push_file(input_filename)) {
            return EXIT_FAILURE; // error already reported
        }
    }

    std::string result;
    if (pp.run(result)) {
        *out << result;
    }

    return g_error_reporter.has_errors()
           ? EXIT_FAILURE
           : EXIT_SUCCESS;
}
