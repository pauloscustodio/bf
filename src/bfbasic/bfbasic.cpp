//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "symbols.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void error(const std::string& msg) {
    std::cerr << "Error: " << msg << std::endl;
    exit(EXIT_FAILURE);
}

void usage_error() {
    std::cerr << "usage: bfbasic [-o output_file] input_file" << std::endl;
    exit(EXIT_FAILURE);
}

std::string read_file(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        error("cannot open input file: " + filename);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void collect_expr_symbols(const Expr& e, SymbolTable& sym) {
    if (e.type == Expr::Type::Var) {
        sym.declare(e.name);
    }

    if (e.type == Expr::Type::BinOp) {
        collect_expr_symbols(*e.left, sym);
        collect_expr_symbols(*e.right, sym);
    }
}

void collect_symbols(const Program& prog, SymbolTable& sym) {
    for (const Stmt& s : prog.statements) {
        sym.declare(s.var);

        if (s.type == Stmt::Type::Let) {
            collect_expr_symbols(*s.expr, sym);
        }
    }
}

std::string compile(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Program prog = parser.parse_program();

    SymbolTable sym;
    collect_symbols(prog, sym);

    CodeGen cg(sym);
    return cg.generate(prog);
}

int main(int argc, char* argv[]) {
    std::string input_filename;
    std::string output_filename;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.empty()) {
            continue;
        }

        // handle -o output_file
        if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "bfpp: missing filename after -o" << std::endl;
                return EXIT_FAILURE;
            }
            output_filename = argv[++i];
            continue;
        }

        // unknown option
        if (arg[0] == '-') {
            usage_error();
        }

        // filename argument
        if (input_filename.empty()) {
            input_filename = arg;
            continue;
        }

        usage_error();
    }

    // open output stream
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

    // read input stream or input file
    std::string source_text;
    if (input_filename.empty()) {
        source_text.assign(std::istreambuf_iterator<char>(std::cin),
                           std::istreambuf_iterator<char>());
    }
    else {
        source_text = read_file(input_filename);
    }

    // parse and compile source text
    std::string bfpp_code = compile(source_text);

    // output to output_file or std::cout
    *out << bfpp_code;
}
