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

void collect_symbols_in_expr(Expr& e, SymbolTable& sym) {
    switch (e.expr_type) {
    case ExprType::Number:
        break;

    case ExprType::Var: {
        bool is_string = is_string_var(e.name);
        SymbolType type = is_string ? SymbolType::StringVar : SymbolType::IntVar;
        sym.declare(e.name, type, e.loc);
        break;
    }
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

void collect_symbols(StmtList& prog, SymbolTable& sym) {
    for (auto& stmt : prog.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            switch (stmt->let_stmt->type) {
            case LetType::Normal: {
                std::string name = stmt->let_stmt->var;
                bool is_string = is_string_var(name);
                SymbolType type = is_string ?
                                  SymbolType::StringVar : SymbolType::IntVar;

                sym.declare(name, type, stmt->loc);

                Symbol* symbol = sym.get(name);
                symbol->count_assignments++;
                break;
            }
            case LetType::Array: {
                // must already have been declared
                std::string name = stmt->let_stmt->var;
                Symbol* symbol = sym.get(name);
                if (!symbol) {
                    error_at(
                        stmt->loc,
                        "array '" + name + "' must be declared by DIM before used");
                }
                if (symbol->type != SymbolType::IntArrayVar) {
                    error_at(
                        stmt->loc,
                        "variable '" + name + "' is not an array");
                }
                break;
            }
            case LetType::String: {
                // must already have been declared
                std::string name = stmt->let_stmt->var;
                Symbol* symbol = sym.get(name);
                if (!symbol) {
                    error_at(
                        stmt->loc,
                        "string variable '" + name + "' must be declared by DIM before used");
                }
                if (symbol->type != SymbolType::StringVar) {
                    error_at(
                        stmt->loc,
                        "variable '" + name + "' is not a string variable");
                }
                break;
            }
            }

            collect_symbols_in_expr(stmt->let_stmt->expr, sym);
            break;

        case StmtType::Dim: {
            std::string name = stmt->dim_stmt->var;
            bool is_string = is_string_var(name);
            SymbolType type = is_string ?
                              SymbolType::StringVar : SymbolType::IntArrayVar;

            auto folded = fold_const_int_expr(*stmt->dim_stmt->size_expr);
            if (!folded) {
                error_at(stmt->loc, "DIM size must be a constant expression");
            }

            int size = *folded;
            if (size <= 0) {
                error_at(stmt->loc, "DIM size must be > 0");
            }

            sym.declare(name, type, stmt->loc);

            Symbol* symbol = sym.get(name);
            symbol->array_size = size;

            break;
        }
        case StmtType::Input:
            for (auto& name : stmt->input_stmt->vars) {
                bool is_string = is_string_var(name);
                SymbolType type = is_string ?
                                  SymbolType::StringVar : SymbolType::IntVar;

                sym.declare(name, type, stmt->loc);

                Symbol* symbol = sym.get(name);
                symbol->count_assignments++;
            }
            break;

        case StmtType::Print:
            for (auto& item : stmt->print_stmt->elems) {
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
            std::string name = stmt->for_stmt->var;
            bool is_string = is_string_var(name);
            if (is_string) {
                error_at(
                    stmt->loc,
                    "loop variable '" + name + "' cannot be a string variable");
            }

            sym.declare(name, SymbolType::IntVar, stmt->loc);

            Symbol* symbol = sym.get(name);
            symbol->count_assignments++;

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

// After collect_symbols:
// mark single-assignment constants now that counts are final
void mark_single_assignment_constants(StmtList& prog, SymbolTable& sym) {
    for (const auto& stmt : prog.statements) {
        switch (stmt->type) {
        case StmtType::Let: {
            if (stmt->let_stmt->type == LetType::Normal &&
                    stmt->let_stmt->expr.expr_type == ExprType::Number) {
                // only mark simple variables, not arrays or strings
                std::string name = stmt->let_stmt->var;
                Symbol* symbol = sym.get(name);
                if (symbol &&
                        symbol->type == SymbolType::IntVar &&
                        symbol->count_assignments == 1) {
                    // symbol is constant, can be optimized
                    symbol->const_value = stmt->let_stmt->expr.int_value;
                }
            }
            break;
        }
        case StmtType::If:
            mark_single_assignment_constants(stmt->if_stmt->then_block, sym);
            mark_single_assignment_constants(stmt->if_stmt->else_block, sym);
            break;

        case StmtType::While:
            mark_single_assignment_constants(stmt->while_stmt->body, sym);
            break;

        case StmtType::For:
            mark_single_assignment_constants(stmt->for_stmt->body, sym);
            break;

        default:
            break;
        }
    }
}

void fold_constants_in_expr(Expr& e, SymbolTable& sym) {
    switch (e.expr_type) {
    case ExprType::Number:
    case ExprType::StringLiteral:
        break;

    case ExprType::Var: {
        Symbol* symbol = sym.get(e.name);
        if (symbol && symbol->const_value) {
            e = Expr::number(*symbol->const_value, e.loc);
        }
        break;
    }

    case ExprType::BinOp:
        fold_constants_in_expr(*e.left, sym);
        fold_constants_in_expr(*e.right, sym);
        if (auto folded = fold_const_int_expr(e)) {
            e = Expr::number(*folded, e.loc);
        }
        break;

    case ExprType::UnaryOp:
        fold_constants_in_expr(*e.inner, sym);
        if (auto folded = fold_const_int_expr(e)) {
            e = Expr::number(*folded, e.loc);
        }
        break;

    case ExprType::ArrayAccess:
        fold_constants_in_expr(*e.index, sym);
        break;

    case ExprType::Concat:
        fold_constants_in_expr(*e.left, sym);
        fold_constants_in_expr(*e.right, sym);
        break;

    case ExprType::Call:
        for (auto& arg : e.args) {
            fold_constants_in_expr(*arg, sym);
        }
        break;

    default:
        assert(0);
    }
}

void fold_constants(StmtList& prog, SymbolTable& sym) {
    for (auto& stmt : prog.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            if (stmt->let_stmt->index) {
                fold_constants_in_expr(*stmt->let_stmt->index, sym);
            }
            fold_constants_in_expr(stmt->let_stmt->expr, sym);
            break;

        case StmtType::Dim:
            fold_constants_in_expr(*stmt->dim_stmt->size_expr, sym);
            break;

        case StmtType::Input:
            break;

        case StmtType::Print:
            for (auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    fold_constants_in_expr(item.expr, sym);
                }
            }
            break;

        case StmtType::If:
            fold_constants_in_expr(stmt->if_stmt->condition, sym);
            fold_constants(stmt->if_stmt->then_block, sym);
            fold_constants(stmt->if_stmt->else_block, sym);
            break;

        case StmtType::While:
            fold_constants_in_expr(stmt->while_stmt->condition, sym);
            fold_constants(stmt->while_stmt->body, sym);
            break;

        case StmtType::For:
            fold_constants_in_expr(stmt->for_stmt->start_expr, sym);
            fold_constants_in_expr(stmt->for_stmt->end_expr, sym);
            fold_constants_in_expr(stmt->for_stmt->step_expr, sym);
            fold_constants(stmt->for_stmt->body, sym);
            break;
        default:
            assert(0);
        }
    }
}

void semantic_check_in_expr(Expr& e, SymbolTable& sym) {
    switch (e.expr_type) {
    case ExprType::Number:
        assert(e.value_type == ValueType::Int);
        break;

    case ExprType::StringLiteral:
        assert(e.value_type == ValueType::String);
        e.string_size = static_cast<int>(e.string_value.size());
        break;

    case ExprType::Var: {
        Symbol* symbol = sym.get(e.name);
        assert(symbol);  // should have been caught in collect_symbols
        bool is_string = is_string_var(e.name);
        e.value_type = is_string ? ValueType::String : ValueType::Int;
        break;
    }
    case ExprType::BinOp:
        semantic_check_in_expr(*e.left, sym);
        semantic_check_in_expr(*e.right, sym);
        if (e.left->value_type == ValueType::String ||
                e.right->value_type == ValueType::String) {
            error_at(
                e.loc,
                "operator '" + token_type_to_string(e.op) + "' cannot be applied to strings");
        }

        e.value_type = ValueType::Int;
        break;

    case ExprType::UnaryOp:
        semantic_check_in_expr(*e.inner, sym);
        if (e.inner->value_type == ValueType::String) {
            error_at(
                e.loc,
                "operator '" + token_type_to_string(e.op) + "' cannot be applied to strings");
        }

        e.value_type = ValueType::Int;
        break;

    case ExprType::ArrayAccess: {
        // assertions for errors that should have been caught in collect_symbols
        Symbol* symbol = sym.get(e.name);
        assert(symbol);
        bool is_string = is_string_var(e.name);
        assert(!is_string);
        assert(symbol->type == SymbolType::IntArrayVar);

        e.value_type = ValueType::Int;

        semantic_check_in_expr(*e.index, sym);
        break;
    }
    case ExprType::Concat:
        semantic_check_in_expr(*e.left, sym);
        semantic_check_in_expr(*e.right, sym);
        if (e.left->value_type == ValueType::Int ||
                e.right->value_type == ValueType::Int) {
            error_at(
                e.loc,
                "operator '" + token_type_to_string(e.op) + "' cannot be applied to numbers");
        }

        e.value_type = ValueType::String;
        e.string_size = e.left->string_size + e.right->string_size;
        break;

    case ExprType::Call:
        for (const auto& arg : e.args) {
            semantic_check_in_expr(*arg, sym);
        }

        switch (e.func) {
        case TokenType::KeywordLeftDollar:
            if (e.args.size() != 2) {
                error_at(e.loc, "LEFT$ function takes exactly 2 arguments");
            }
            if (e.args[0]->value_type == ValueType::Int) {
                error_at(e.loc, "LEFT$ function's first argument cannot be a number");
            }
            if (e.args[1]->value_type == ValueType::String) {
                error_at(e.loc, "LEFT$ function's second argument cannot be a string");
            }
            e.value_type = ValueType::String;
            e.string_size = e.args[0]->string_size;  // upper bound; actual size may be smaller
            break;

        case TokenType::KeywordMidDollar:
            if (e.args.size() != 3) {
                error_at(e.loc, "MID$ function takes exactly 3 arguments");
            }
            if (e.args[0]->value_type == ValueType::Int) {
                error_at(e.loc, "MID$ function's first argument cannot be a number");
            }
            if (e.args[1]->value_type == ValueType::String) {
                error_at(e.loc, "MID$ function's second argument cannot be a string");
            }
            if (e.args[2]->value_type == ValueType::String) {
                error_at(e.loc, "MID$ function's third argument cannot be a string");
            }
            e.value_type = ValueType::String;
            e.string_size = e.args[0]->string_size;  // upper bound; actual size may be smaller
            break;

        case TokenType::KeywordRightDollar:
            if (e.args.size() != 2) {
                error_at(e.loc, "RIGHT$ function takes exactly 2 arguments");
            }
            if (e.args[0]->value_type == ValueType::Int) {
                error_at(e.loc, "RIGHT$ function's first argument cannot be a number");
            }
            if (e.args[1]->value_type == ValueType::String) {
                error_at(e.loc, "RIGHT$ function's second argument cannot be a string");
            }
            e.value_type = ValueType::String;
            e.string_size = e.args[0]->string_size;  // upper bound; actual size may be smaller
            break;

        case TokenType::KeywordStrDollar:
            if (e.args.size() != 1) {
                error_at(e.loc, "STR$ function takes exactly 1 argument");
            }
            if (e.args[0]->value_type == ValueType::String) {
                error_at(e.loc, "STR$ function's argument cannot be a string");
            }
            e.value_type = ValueType::String;
            e.string_size = 1 + 5 + 1; // upper bound for 16-bit int: -32768 space
            break;

        case TokenType::KeywordLen:
            if (e.args.size() != 1) {
                error_at(e.loc, "LEN function takes exactly 1 argument");
            }
            if (e.args[0]->value_type == ValueType::Int) {
                error_at(e.loc, "LEN function's argument cannot be a number");
            }
            e.value_type = ValueType::Int;
            break;

        case TokenType::KeywordVal:
            if (e.args.size() != 1) {
                error_at(e.loc, "VAL function takes exactly 1 argument");
            }
            if (e.args[0]->value_type == ValueType::Int) {
                error_at(e.loc, "VAL function's argument cannot be a number");
            }
            e.value_type = ValueType::Int;
            break;

        case TokenType::KeywordChrDollar:
            if (e.args.size() != 1) {
                error_at(e.loc, "CHR$ function takes exactly 1 argument");
            }
            if (e.args[0]->value_type == ValueType::String) {
                error_at(e.loc, "CHR$ function's argument cannot be a string");
            }
            e.value_type = ValueType::String;
            e.string_size = 1;
            break;

        case TokenType::KeywordAsc:
            if (e.args.size() != 1) {
                error_at(e.loc, "ASC function takes exactly 1 argument");
            }
            if (e.args[0]->value_type == ValueType::Int) {
                error_at(e.loc, "ASC function's argument cannot be a number");
            }
            e.value_type = ValueType::Int;
            break;

        default:
            assert(0);
        }

        break;

    default:
        assert(0);
    }
}

void semantic_check(StmtList& prog, SymbolTable& sym) {
    for (auto& stmt : prog.statements) {
        switch (stmt->type) {
        case StmtType::Let: {
            if (stmt->let_stmt->index) {
                semantic_check_in_expr(*stmt->let_stmt->index, sym);
            }
            semantic_check_in_expr(stmt->let_stmt->expr, sym);

            switch (stmt->let_stmt->type) {
            case LetType::Normal:
                if (stmt->let_stmt->expr.value_type == ValueType::String) {
                    error_at(
                        stmt->loc,
                        "cannot assign string to number variable '" + stmt->let_stmt->var + "'");
                }
                break;

            case LetType::Array:
                if (stmt->let_stmt->expr.value_type == ValueType::String) {
                    error_at(
                        stmt->loc,
                        "cannot assign string to array variable '" + stmt->let_stmt->var + "'");
                }
                break;

            case LetType::String:
                if (stmt->let_stmt->expr.value_type == ValueType::Int) {
                    error_at(
                        stmt->loc,
                        "cannot assign number to string variable '" + stmt->let_stmt->var + "'");
                }
                break;

            default:
                assert(0);
            }
            break;
        }
        case StmtType::Dim:
            semantic_check_in_expr(*stmt->dim_stmt->size_expr, sym);
            break;

        case StmtType::Input:
            break;

        case StmtType::Print:
            for (auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    semantic_check_in_expr(item.expr, sym);
                }
            }
            break;

        case StmtType::If:
            semantic_check_in_expr(stmt->if_stmt->condition, sym);
            semantic_check(stmt->if_stmt->then_block, sym);
            semantic_check(stmt->if_stmt->else_block, sym);
            break;

        case StmtType::While:
            semantic_check_in_expr(stmt->while_stmt->condition, sym);
            semantic_check(stmt->while_stmt->body, sym);
            break;

        case StmtType::For:
            semantic_check_in_expr(stmt->for_stmt->start_expr, sym);
            semantic_check_in_expr(stmt->for_stmt->end_expr, sym);
            semantic_check_in_expr(stmt->for_stmt->step_expr, sym);
            semantic_check(stmt->for_stmt->body, sym);
            break;
        default:
            assert(0);
        }
    }
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
    mark_single_assignment_constants(prog, sym);
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
