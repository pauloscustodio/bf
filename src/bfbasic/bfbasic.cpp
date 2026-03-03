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
#include "utils.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

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

std::optional<int> fold_const_int_expr(const Expr& e) {
    switch (e.expr_type) {

    case ExprType::Number:
        return e.int_value;

    case ExprType::Var:
        return std::nullopt;   // variables are not constant

    case ExprType::BinOp: {
        auto L = fold_const_int_expr(*e.left);
        auto R = fold_const_int_expr(*e.right);
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
            return (*L < *R) ? 1 : 0;
        case TokenType::LessEqual:
            return (*L <= *R) ? 1 : 0;
        case TokenType::Greater:
            return (*L > *R) ? 1 : 0;
        case TokenType::GreaterEqual:
            return (*L >= *R) ? 1 : 0;
        case TokenType::KeywordAnd:
            return (*L && *R) ? 1 : 0;
        case TokenType::KeywordOr:
            return (*L || *R) ? 1 : 0;
        case TokenType::KeywordXor:
            return (!!*L != !!*R) ? 1 : 0;
        default:
            assert(0);
        }
        break;
    }

    case ExprType::UnaryOp: {
        auto inner = fold_const_int_expr(*e.inner);
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
            assert(0);
        }
        break;
    }

    case ExprType::ArrayAccess:
        return std::nullopt;   // arrays are not constant

    case ExprType::StringLiteral:
    case ExprType::Concat:
        return std::nullopt;   // strings not folded here

    case ExprType::Call:
        return std::nullopt;   // functions not folded here

    default:
        assert(0);
    }

    return std::nullopt;
}

void const_int_expr_evaluator(StmtList& prog) {
    for (auto& stmt : prog.statements) {
        switch (stmt->type) {

        case StmtType::Let: {
            if (stmt->let_stmt->index) {
                auto folded_idx = fold_const_int_expr(*stmt->let_stmt->index);
                if (folded_idx) {
                    stmt->let_stmt->index =
                        std::make_unique<Expr>(Expr::number(*folded_idx, stmt->loc));
                }
            }

            auto folded = fold_const_int_expr(stmt->let_stmt->expr);
            if (folded) {
                stmt->let_stmt->expr = Expr::number(*folded, stmt->loc);
            }
            break;
        }

        case StmtType::Dim: {
            auto folded = fold_const_int_expr(*stmt->dim_stmt->size_expr);
            if (folded) {
                stmt->dim_stmt->size_expr =
                    std::make_unique<Expr>(Expr::number(*folded, stmt->loc));
            }
            break;
        }

        case StmtType::Input:
            break;  // no expressions to fold

        case StmtType::Print: {
            for (auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    auto folded = fold_const_int_expr(item.expr);
                    if (folded) {
                        item.expr = Expr::number(*folded, stmt->loc);
                    }
                }
            }
            break;
        }

        case StmtType::If: {
            auto folded = fold_const_int_expr(stmt->if_stmt->condition);
            if (folded) {
                stmt->if_stmt->condition = Expr::number(*folded, stmt->loc);
            }

            // recurse
            const_int_expr_evaluator(stmt->if_stmt->then_block);
            const_int_expr_evaluator(stmt->if_stmt->else_block);
            break;
        }

        case StmtType::While: {
            auto folded = fold_const_int_expr(stmt->while_stmt->condition);
            if (folded) {
                stmt->while_stmt->condition = Expr::number(*folded, stmt->loc);
            }

            // recurse
            const_int_expr_evaluator(stmt->while_stmt->body);
            break;
        }

        case StmtType::For: {
            auto folded = fold_const_int_expr(stmt->for_stmt->start_expr);
            if (folded) {
                stmt->for_stmt->start_expr = Expr::number(*folded, stmt->loc);
            }
            folded = fold_const_int_expr(stmt->for_stmt->end_expr);
            if (folded) {
                stmt->for_stmt->end_expr = Expr::number(*folded, stmt->loc);
            }
            folded = fold_const_int_expr(stmt->for_stmt->step_expr);
            if (folded) {
                stmt->for_stmt->step_expr = Expr::number(*folded, stmt->loc);
            }

            // recurse
            const_int_expr_evaluator(stmt->for_stmt->body);
            break;
        }

        default:
            assert(0);
        }
    }
}

void collect_symbols_in_expr(const Expr& e, SymbolTable& sym) {
    switch (e.expr_type) {
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

    case ExprType::StringLiteral:
        break;

    case ExprType::Concat:
        collect_symbols_in_expr(*e.left, sym);
        collect_symbols_in_expr(*e.right, sym);
        break;

    case ExprType::Call:
        for (const auto& arg : e.args) {
            collect_symbols_in_expr(*arg, sym);
        }
        break;

    default:
        assert(0);
    }
}

void collect_symbols(const StmtList& prog, SymbolTable& sym) {
    for (const auto& stmt : prog.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            if (stmt->let_stmt->is_array) {
                // must already have been declared
                std::string name = stmt->let_stmt->var;
                if (!sym.exists(name)) {
                    error_at(stmt->loc, "array '" + name + "' must be declared by DIM before used");
                }
                Symbol& symbol = sym.get(name);
                if (!symbol.is_array) {
                    error_at(stmt->loc, "variable '" + name + "' is not an array");
                }
            }
            else {
                sym.declare_variable(stmt->loc, stmt->let_stmt->var);
                Symbol& symbol = sym.get(stmt->let_stmt->var);
                symbol.count_assignments++;
            }
            collect_symbols_in_expr(stmt->let_stmt->expr, sym);
            break;

        case StmtType::Dim: {
            auto folded = fold_const_int_expr(*stmt->dim_stmt->size_expr);
            if (!folded) {
                error_at(stmt->loc, "DIM size must be a constant expression");
            }

            int size = *folded;
            if (size <= 0) {
                error_at(stmt->loc, "DIM size must be > 0");
            }

            // BASIC 1-based
            sym.declare_array(stmt->loc, stmt->dim_stmt->var, size + 1);
            break;
        }
        case StmtType::Input:
            for (auto& v : stmt->input_stmt->vars) {
                sym.declare_variable(stmt->loc, v);
                Symbol& symbol = sym.get(v);
                symbol.count_assignments++;
            }
            break;

        case StmtType::Print:
            for (const auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    collect_symbols_in_expr(item.expr, sym);
                }
            }
            break;

        case StmtType::If:
            collect_symbols_in_expr(stmt->if_stmt->condition, sym);
            collect_symbols(stmt->if_stmt->then_block, sym);
            collect_symbols(stmt->if_stmt->else_block, sym);
            break;

        case StmtType::While:
            collect_symbols_in_expr(stmt->while_stmt->condition, sym);
            collect_symbols(stmt->while_stmt->body, sym);
            break;

        case StmtType::For: {
            sym.declare_variable(stmt->loc, stmt->for_stmt->var);
            Symbol& symbol = sym.get(stmt->for_stmt->var);
            symbol.count_assignments++;

            collect_symbols_in_expr(stmt->for_stmt->start_expr, sym);
            collect_symbols_in_expr(stmt->for_stmt->end_expr, sym);
            collect_symbols_in_expr(stmt->for_stmt->step_expr, sym);
            collect_symbols(stmt->for_stmt->body, sym);
            break;
        }
        default:
            assert(0);
        }
    }
}

void fold_constants(StmtList& prog, const SymbolTable& sym) {
    (void)prog;
    (void)sym;
    /*
    for (auto& stmt : prog.statements) {
        if (stmt->type == StmtType::Let) {
            auto folded = fold_const_int_expr(stmt->let_stmt->expr);
            if (folded) {
                stmt->let_stmt->expr = Expr::number(*folded, stmt->loc);
            }
        }
    }
    */
}

void semantic_check(const StmtList& prog, const SymbolTable& sym) {
    (void)prog;
    (void)sym;
    // currently all semantic checks are done during symbol collection and
    // constant folding, so nothing to do here
}

std::string compile(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Program prog = parser.parse_program();

    // need to fold constant integer expressions before symbol collection,
    // because array sizes in DIM statements must be constant expressions
    const_int_expr_evaluator(prog);

    SymbolTable sym;
    collect_symbols(prog, sym);
    fold_constants(prog, sym);
    semantic_check(prog, sym);

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
