//-----------------------------------------------------------------------------
// Brainfuck BASIC compiler
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "ast.h"
#include "codegen.h"
#include "errors.h"
#include "lexer.h"
#include "parser.h"
#include "symbols.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

std::optional<int> fold_constant_expr(const Expr& e);

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

void collect_symbols_in_expr(const Expr& e, SymbolTable& sym) {
    switch (e.type) {
    case ExprType::Number:
        break;

    case ExprType::Var:
        sym.declare_variable(e.loc, e.name);
        break;

    case ExprType::BinOp:
        collect_symbols_in_expr(*e.left, sym);
        collect_symbols_in_expr(*e.right, sym);
        break;

    case ExprType::UnaryOp:
        collect_symbols_in_expr(*e.inner, sym);
        break;

    case ExprType::ArrayAccess:
        collect_symbols_in_expr(*e.index, sym);
        break;

    default:
        assert(0);
    }
}

void collect_symbols_in_stmt(const Stmt& s, SymbolTable& sym) {
    switch (s.type) {
    case StmtType::Let:
        if (s.let_stmt->is_array) {
            // must already have been declared
            std::string name = s.let_stmt->var;
            if (!sym.exists(name)) {
                error_at(s.loc, "array '" + name + "' must be declared by DIM before used");
            }
            Symbol symbol = sym.get(name);
            if (!symbol.is_array) {
                error_at(s.loc, "variable '" + name + "' is not an array");
            }
        }
        else {
            sym.declare_variable(s.loc, s.let_stmt->var);
        }
        collect_symbols_in_expr(s.let_stmt->expr, sym);
        break;

    case StmtType::Dim: {
        auto folded = fold_constant_expr(*s.dim_stmt->size_expr);
        if (!folded) {
            error_at(s.loc, "DIM size must be a constant expression");
        }

        int size = *folded;
        if (size <= 0) {
            error_at(s.loc, "DIM size must be > 0");
        }

        sym.declare_array(s.loc, s.dim_stmt->var, size + 1); // BASIC 1-based
        break;
    }
    case StmtType::Input:
        for (auto& v : s.input_stmt->vars) {
            sym.declare_variable(s.loc, v);
        }
        break;

    case StmtType::Print:
        for (const auto& item : s.print_stmt->elems) {
            if (item.type == PrintElemType::Expr) {
                collect_symbols_in_expr(item.expr, sym);
            }
        }
        break;

    case StmtType::If:
        collect_symbols_in_expr(s.if_stmt->condition, sym);
        for (const auto& stmt : s.if_stmt->then_block.statements) {
            collect_symbols_in_stmt(*stmt, sym);
        }
        for (const auto& stmt : s.if_stmt->else_block.statements) {
            collect_symbols_in_stmt(*stmt, sym);
        }
        break;

    case StmtType::While:
        collect_symbols_in_expr(s.while_stmt->condition, sym);
        for (const auto& stmt : s.while_stmt->body.statements) {
            collect_symbols_in_stmt(*stmt, sym);
        }
        break;

    case StmtType::For:
        sym.declare_variable(s.loc, s.for_stmt->var);
        collect_symbols_in_expr(s.for_stmt->start_expr, sym);
        collect_symbols_in_expr(s.for_stmt->end_expr, sym);
        collect_symbols_in_expr(s.for_stmt->step_expr, sym);
        for (auto& stmt : s.for_stmt->body.statements) {
            collect_symbols_in_stmt(*stmt, sym);
        }
        break;

    default:
        assert(0);
    }
}

void collect_symbols(const Program& prog, SymbolTable& sym) {
    for (const auto& s : prog.statements) {
        collect_symbols_in_stmt(*s, sym);
    }
}

int ipow(int base, int exp) {
    if (exp < 0) {
        return 0; // or handle error for negative exponents
    }

    int result = 1;
    while (exp > 0) {
        if (exp & 1) {
            result *= base;
        }
        base *= base;
        exp >>= 1;
    }
    return result;
}

std::optional<int> fold_constant_expr(const Expr& e) {
    switch (e.type) {

    case ExprType::Number:
        return e.value;

    case ExprType::Var:
    case ExprType::ArrayAccess:
        return std::nullopt;   // variables are not constant

    case ExprType::UnaryOp: {
        auto inner = fold_constant_expr(*e.inner);
        if (!inner) {
            return std::nullopt;
        }

        switch (e.op) {
        case TokenType::Minus:
            return -(*inner);
        case TokenType::Plus:
            return +(*inner);
        case TokenType::KeywordNot:
            return (*inner == 0) ? 1 : 0;
        default:
            return std::nullopt;
        }
    }

    case ExprType::BinOp: {
        auto L = fold_constant_expr(*e.left);
        auto R = fold_constant_expr(*e.right);
        if (!L || !R) {
            return std::nullopt;
        }

        switch (e.op) {
        case TokenType::Plus:
            return *L + *R;
        case TokenType::Minus:
            return *L - *R;
        case TokenType::Star:
            return *L * *R;
        case TokenType::Slash:
            if (*R == 0) {
                error_at(e.loc, "Division by zero in constant expression");
            }
            return *L / *R;
        case TokenType::KeywordMod:
            if (*R == 0) {
                error_at(e.loc, "Modulo by zero in constant expression");
            }
            return *L % *R;
        case TokenType::Caret:
            return ipow(*L, *R);
        case TokenType::KeywordShl:
            return *L << *R;
        case TokenType::KeywordShr:
            return *L >> *R;
        case TokenType::Equal:
            return (*L == *R) ? 1 : 0;
        case TokenType::NotEqual:
            return (*L != *R) ? 1 : 0;
        case TokenType::Less:
            return (*L <  *R) ? 1 : 0;
        case TokenType::LessEqual:
            return (*L <= *R) ? 1 : 0;
        case TokenType::Greater:
            return (*L >  *R) ? 1 : 0;
        case TokenType::GreaterEqual:
            return (*L >= *R) ? 1 : 0;
        case TokenType::KeywordAnd:
            return (*L && *R) ? 1 : 0;
        case TokenType::KeywordOr:
            return (*L || *R) ? 1 : 0;
        case TokenType::KeywordXor:
            return (!!*L != !!*R) ? 1 : 0;
        default:
            return std::nullopt;
        }
    }
    }

    return std::nullopt;
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
