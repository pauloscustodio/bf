//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "files.h"
#include "lexer.h"
#include "macros.h"
#include "preprocessor.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    std::string input_filename;
    std::string output_filename;
    std::vector<Macro> cmd_macros;
    bool verbose = false;

    // --- Parse command-line arguments ---
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // handle -o output_file
        if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "bfpp: missing filename after -o" << std::endl;
                return EXIT_FAILURE;
            }
            output_filename = argv[++i];
            continue;
        }

        // verbose
        if (arg == "-v") {
            verbose = true;
            continue;
        }

        // Handle -I path or -Ipath
        if (arg == "-I" || (arg.length() > 2 && arg.substr(0, 2) == "-I")) {
            std::string path;
            if (arg == "-I") {
                if (i + 1 >= argc) {
                    std::cerr << "bfpp: missing path after -I" << std::endl;
                    return EXIT_FAILURE;
                }
                path = argv[++i];
            }
            else {
                path = arg.substr(2); // -Ipath
            }
            if (path.empty()) {
                std::cerr << "bfpp: empty include path" << std::endl;
                return EXIT_FAILURE;
            }
            g_file_stack.add_include_path(path);
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
            if (!is_identifier(name)) {
                std::cerr << "bfpp: invalid macro name: " << name << std::endl;
                return EXIT_FAILURE;
            }

            if (is_reserved_keyword(name)) {
                std::cerr << "bfpp: macro name is a reserved keyword: " << name << std::endl;
                return EXIT_FAILURE;
            }

            // validate value is an integer
            if (!is_integer(value_str)) {
                std::cerr << "bfpp: invalid integer value: " << value_str << std::endl;
                return EXIT_FAILURE;
            }

            // Parse integer value
            int value = std::stoi(value_str);

            // Create macro with single integer token
            Macro macro;
            macro.name = name;
            macro.params = {};  // object-like macro
            macro.body.push_back(Token::make_int(value,
                                                 SourceLocation("<command-line>", 0, 0)));
            macro.loc = SourceLocation("<command-line>", 0, 0);

            g_macro_table.define(macro);
            cmd_macros.push_back(macro);
            continue;
        }

        if (arg[0] == '-') {
            std::cerr << "usage: bfpp [-o output_file] [-I include_path] [-D name=value] [-v] [input_file]" << std::endl;
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

    // read entry
    const bool from_stdin = input_filename.empty();
    std::string stdin_buf;
    if (from_stdin) {
        stdin_buf.assign(std::istreambuf_iterator<char>(std::cin),
                         std::istreambuf_iterator<char>());
    }

    auto restore_cmd_macros = [&]() {
        g_macro_table.clear();
        for (const auto& m : cmd_macros) {
            g_macro_table.define(m);
        }
    };

    auto run_pass = [&](int stack_base,
                        std::string & out_text,
                        int& heap_out,
    int& depth_out) -> bool {
        restore_cmd_macros();
        reset_temp_names();
        g_file_stack.reset();
        Preprocessor pp;
        if (stack_base >= 0) {
            pp.set_stack_base(stack_base);
        }
        if (from_stdin) {
            std::istringstream ss(stdin_buf);
            pp.push_stream(ss, "<stdin>");
        }
        else {
            if (!pp.push_file(input_filename)) {
                return false;
            }
        }
        bool ok = pp.run(out_text);
        heap_out = pp.heap_size();
        depth_out = pp.max_stack_depth();
        return ok;
    };

    // Pass 1
    std::string pass1;
    int heap1 = 0, depth1 = 0;
    if (!run_pass(/*stack_base=*/ -1, pass1, heap1, depth1)) {
        return EXIT_FAILURE;
    }
    int stack_base = heap1 + BFOutput::kMinHeapToStackDistance + depth1;

    // Pass 2
    std::string pass2;
    int heap2 = 0, depth2 = 0;
    if (!run_pass(stack_base, pass2, heap2, depth2)) {
        return EXIT_FAILURE;
    }
    *out << pass2;

    if (verbose) {
        std::cerr << "heap=" << heap2
                  << " stack=" << depth2
                  << " stack_base=" << stack_base
                  << std::endl;
    }

    return g_error_reporter.has_errors()
           ? EXIT_FAILURE
           : EXIT_SUCCESS;
}
