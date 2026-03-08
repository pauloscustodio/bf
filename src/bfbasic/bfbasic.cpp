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
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>

// Global inline instance counter
static int g_inline_counter = 0;

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

//-----------------------------------------------------------------------------
// Reclassify ArrayAccess nodes that are actually user-defined function calls.
// The parser cannot distinguish F(x) as array vs function at parse time for
// single-argument calls, so we fix it here using prog.functions.
//-----------------------------------------------------------------------------

static void reclassify_func_calls_in_expr(Expr& e, const Program& prog) {
    switch (e.expr_type) {
    case ExprType::Number:
    case ExprType::StringLiteral:
    case ExprType::Var:
        break;

    case ExprType::BinOp:
    case ExprType::RelOp:
    case ExprType::Concat:
        reclassify_func_calls_in_expr(*e.left, prog);
        reclassify_func_calls_in_expr(*e.right, prog);
        break;

    case ExprType::UnaryOp:
        reclassify_func_calls_in_expr(*e.inner, prog);
        break;

    case ExprType::ArrayAccess:
        reclassify_func_calls_in_expr(*e.index, prog);
        // Check if this is actually a user-defined function call
        if (prog.functions.count(e.name)) {
            // Reclassify: ArrayAccess(name, index) -> Call(name, [index])
            e.expr_type = ExprType::Call;
            e.func = TokenType::Identifier;
            e.args.push_back(std::move(e.index));
            e.index.reset();
        }
        break;

    case ExprType::Call:
        for (auto& arg : e.args) {
            reclassify_func_calls_in_expr(*arg, prog);
        }
        break;

    default:
        break;
    }
}

static void reclassify_func_calls(StmtList& stmts, const Program& prog) {
    for (auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            if (stmt->let_stmt->index) {
                reclassify_func_calls_in_expr(*stmt->let_stmt->index, prog);
            }
            reclassify_func_calls_in_expr(stmt->let_stmt->expr, prog);
            break;

        case StmtType::Dim:
            for (auto& elem : stmt->dim_stmt->elems) {
                reclassify_func_calls_in_expr(*elem.size_expr, prog);
            }
            break;

        case StmtType::Input:
            break;

        case StmtType::Print:
            for (auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    reclassify_func_calls_in_expr(item.expr, prog);
                }
            }
            break;

        case StmtType::If:
            reclassify_func_calls_in_expr(stmt->if_stmt->condition, prog);
            reclassify_func_calls(stmt->if_stmt->then_block, prog);
            reclassify_func_calls(stmt->if_stmt->else_block, prog);
            break;

        case StmtType::While:
            reclassify_func_calls_in_expr(stmt->while_stmt->condition, prog);
            reclassify_func_calls(stmt->while_stmt->body, prog);
            break;

        case StmtType::For:
            reclassify_func_calls_in_expr(stmt->for_stmt->start_expr, prog);
            reclassify_func_calls_in_expr(stmt->for_stmt->end_expr, prog);
            reclassify_func_calls_in_expr(stmt->for_stmt->step_expr, prog);
            reclassify_func_calls(stmt->for_stmt->body, prog);
            break;

        case StmtType::Call:
            for (auto& arg : stmt->call_stmt->args) {
                reclassify_func_calls_in_expr(arg, prog);
            }
            break;

        case StmtType::SubDecl:
            break;

        default:
            assert(0);
        }
    }
}

//-----------------------------------------------------------------------------

std::optional<int> fold_const_int_expr(const Expr& e,
                                       const SymbolTable* sym = nullptr) {
    switch (e.expr_type) {

    case ExprType::Number:
        return e.int_value;

    case ExprType::Var: {
        if (sym) {
            const Symbol* s = sym->get(e.name);
            if (s && s->const_value) {
                return *s->const_value;
            }
        }
        return std::nullopt;
    }

    case ExprType::BinOp: {
        auto L = fold_const_int_expr(*e.left, sym);
        auto R = fold_const_int_expr(*e.right, sym);
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

    case ExprType::RelOp: {
        if (e.operand_type == ValueType::String) {
            return std::nullopt;
        }

        auto L = fold_const_int_expr(*e.left, sym);
        auto R = fold_const_int_expr(*e.right, sym);
        if (!L || !R) {
            return std::nullopt;
        }

        switch (e.op) {
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
        default:
            assert(0);
        }
        break;
    }
    case ExprType::UnaryOp: {
        auto inner = fold_const_int_expr(*e.inner, sym);
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
        return std::nullopt;

    case ExprType::StringLiteral:
    case ExprType::Concat:
        return std::nullopt;

    case ExprType::Call:
        return std::nullopt;

    default:
        assert(0);
    }

    return std::nullopt;
}

// Fold string expressions, consulting symbol table for constant vars
std::optional<std::string> fold_const_string_expr(const Expr& e, SymbolTable& sym) {
    switch (e.expr_type) {
    case ExprType::StringLiteral:
        return e.string_value;

    case ExprType::Var: {
        Symbol* s = sym.get(e.name);
        if (s && s->const_string) {
            return *s->const_string;
        }
        return std::nullopt;
    }

    case ExprType::Concat: {
        auto L = fold_const_string_expr(*e.left, sym);
        auto R = fold_const_string_expr(*e.right, sym);
        if (!L || !R) {
            return std::nullopt;
        }
        return *L + *R;
    }

    default:
        return std::nullopt;
    }
}

void collect_symbols_in_expr(Expr& e, SymbolTable& sym, const Program& prog) {
    switch (e.expr_type) {
    case ExprType::Number:
        break;

    case ExprType::Var: {
        if (prog.subroutines.count(e.name) || prog.functions.count(e.name)) {
            error_at(e.loc,
                     "variable '" + e.name + "' conflicts with subroutine or function name");
        }
        bool is_string = is_string_var(e.name);
        SymbolType type = is_string ? SymbolType::StringVar : SymbolType::IntVar;
        sym.declare(e.name, type, e.loc);
        break;
    }
    case ExprType::BinOp:
    case ExprType::RelOp:
    case ExprType::Concat:
        collect_symbols_in_expr(*e.left, sym, prog);
        collect_symbols_in_expr(*e.right, sym, prog);
        break;

    case ExprType::UnaryOp:
        collect_symbols_in_expr(*e.inner, sym, prog);
        break;

    case ExprType::ArrayAccess:
        collect_symbols_in_expr(*e.index, sym, prog);
        break;

    case ExprType::StringLiteral:
        break;

    case ExprType::Call:
        for (const auto& arg : e.args) {
            collect_symbols_in_expr(*arg, sym, prog);
        }
        break;

    default:
        assert(0);
    }
}

void collect_symbols(StmtList& stmts, SymbolTable& sym, const Program& prog) {
    for (auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            switch (stmt->let_stmt->type) {
            case LetType::Normal: {
                std::string name = stmt->let_stmt->var;
                if (prog.subroutines.count(name) || prog.functions.count(name)) {
                    error_at(stmt->loc,
                             "variable '" + name + "' conflicts with subroutine or function name");
                }
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
                symbol->count_assignments++;
                break;
            }
            }

            collect_symbols_in_expr(stmt->let_stmt->expr, sym, prog);
            break;

        case StmtType::Dim: {
            for (auto& elem : stmt->dim_stmt->elems) {
                std::string name = elem.var;
                if (prog.subroutines.count(name) || prog.functions.count(name)) {
                    error_at(stmt->loc,
                             "variable '" + name + "' conflicts with subroutine or function name");
                }
                bool is_string = is_string_var(name);
                SymbolType type = is_string ?
                                  SymbolType::StringVar : SymbolType::IntArrayVar;

                // Try to fold now; if not yet possible, defer check to finalize_array_sizes
                auto folded = fold_const_int_expr(*elem.size_expr);

                sym.declare(name, type, stmt->loc);

                // 0 array_size means "unresolved yet"
                Symbol* symbol = sym.get(name);
                symbol->array_size = folded ? *folded : 0;
            }
            break;
        }
        case StmtType::Input:
            for (auto& name : stmt->input_stmt->vars) {
                if (prog.subroutines.count(name) || prog.functions.count(name)) {
                    error_at(stmt->loc,
                             "variable '" + name + "' conflicts with subroutine or function name");
                }
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
                    collect_symbols_in_expr(item.expr, sym, prog);
                }
            }
            break;

        case StmtType::If:
            collect_symbols_in_expr(stmt->if_stmt->condition, sym, prog);
            collect_symbols(stmt->if_stmt->then_block, sym, prog);
            collect_symbols(stmt->if_stmt->else_block, sym, prog);
            break;

        case StmtType::While:
            collect_symbols_in_expr(stmt->while_stmt->condition, sym, prog);
            collect_symbols(stmt->while_stmt->body, sym, prog);
            break;

        case StmtType::For: {
            std::string name = stmt->for_stmt->var;
            if (prog.subroutines.count(name) || prog.functions.count(name)) {
                error_at(stmt->loc,
                         "variable '" + name + "' conflicts with subroutine or function name");
            }
            bool is_string = is_string_var(name);
            if (is_string) {
                error_at(
                    stmt->loc,
                    "loop variable '" + name + "' cannot be a string variable");
            }

            sym.declare(name, SymbolType::IntVar, stmt->loc);

            Symbol* symbol = sym.get(name);
            symbol->count_assignments++;

            collect_symbols_in_expr(stmt->for_stmt->start_expr, sym, prog);
            collect_symbols_in_expr(stmt->for_stmt->end_expr, sym, prog);
            collect_symbols_in_expr(stmt->for_stmt->step_expr, sym, prog);
            collect_symbols(stmt->for_stmt->body, sym, prog);
            break;
        }
        case StmtType::Call:
            for (auto& arg : stmt->call_stmt->args) {
                collect_symbols_in_expr(arg, sym, prog);
            }
            break;

        case StmtType::SubDecl:
            break;  // subroutines are stored separately, not in the stmt list

        default:
            assert(0);
        }
    }
}

// After collect_symbols:
// mark single-assignment constants now that counts are final
void mark_single_assignment_constants(StmtList& stmts, SymbolTable& sym) {
    for (const auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Let: {
            if (stmt->let_stmt->type == LetType::Normal) {
                std::string name = stmt->let_stmt->var;
                Symbol* symbol = sym.get(name);
                if (symbol &&
                        symbol->type == SymbolType::IntVar &&
                        symbol->count_assignments == 1 &&
                        !symbol->const_value) {
                    // Evaluate RHS using known constants from the symbol table
                    // without mutating the AST.
                    auto folded = fold_const_int_expr(stmt->let_stmt->expr, &sym);
                    if (folded) {
                        symbol->const_value = *folded;
                    }
                }
            }
            else if (stmt->let_stmt->type == LetType::String) {
                std::string name = stmt->let_stmt->var;
                Symbol* symbol = sym.get(name);
                if (symbol &&
                        symbol->type == SymbolType::StringVar &&
                        symbol->count_assignments == 1 &&
                        !symbol->const_string) {
                    auto folded = fold_const_string_expr(stmt->let_stmt->expr, sym);
                    if (folded) {
                        symbol->const_string = *folded;
                    }
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

        case StmtType::Call:
        case StmtType::SubDecl:
        case StmtType::Dim:
        case StmtType::Input:
        case StmtType::Print:
            break;

        default:
            assert(0);
        }
    }
}

void fold_constants_in_expr(Expr& e, SymbolTable& sym) {
    switch (e.expr_type) {
    case ExprType::Number:
    case ExprType::StringLiteral:
        break;

    case ExprType::Var: {
        // need to fold constants here to handle cases like
        // LET A = 1 : LET B = A + 2 : DIM C(B)
        Symbol* symbol = sym.get(e.name);
        if (symbol && symbol->const_value) {
            e = Expr::number(*symbol->const_value, e.loc);
        }
        // Note: don't replace string vars with string literals here.
        // The codegen can reference the variable's cell directly, which is
        // more efficient than materializing a temporary copy of the literal.
        // String constant propagation happens inside fold_const_string_expr
        // when folding Concat nodes.
        break;
    }

    case ExprType::BinOp:
        fold_constants_in_expr(*e.left, sym);
        fold_constants_in_expr(*e.right, sym);
        if (auto folded = fold_const_int_expr(e)) {
            e = Expr::number(*folded, e.loc);
        }
        break;

    case ExprType::RelOp:
        if (e.operand_type == ValueType::String) {
            // string relational ops not folded here
            break;
        }

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
        if (auto folded = fold_const_string_expr(e, sym)) {
            e = Expr::string_literal(*folded, e.loc);
        }
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

void fold_constants(StmtList& stmts, SymbolTable& sym) {
    for (auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            if (stmt->let_stmt->index) {
                fold_constants_in_expr(*stmt->let_stmt->index, sym);
            }
            fold_constants_in_expr(stmt->let_stmt->expr, sym);
            break;

        case StmtType::Dim:
            for (auto& elem : stmt->dim_stmt->elems) {
                fold_constants_in_expr(*elem.size_expr, sym);
            }
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

        case StmtType::Call:
            for (auto& arg : stmt->call_stmt->args) {
                fold_constants_in_expr(arg, sym);
            }
            break;

        case StmtType::SubDecl:
            break;

        default:
            assert(0);
        }
    }
}

// Compute the maximum string_size across all return-value assignments
// in a function body. Does NOT call semantic_check_in_expr; instead it
// estimates string sizes structurally from the raw AST, substituting
// parameter sizes from the call-site argument sizes.
static int estimate_expr_string_size(const Expr& e,
                                     const std::unordered_map<std::string, int>& param_sizes) {
    switch (e.expr_type) {
    case ExprType::StringLiteral:
        return static_cast<int>(e.string_value.size());

    case ExprType::Var: {
        auto it = param_sizes.find(e.name);
        if (it != param_sizes.end()) {
            return it->second;
        }
        return 0; // unknown local; will be sized after inlining
    }

    case ExprType::Concat: {
        int l = estimate_expr_string_size(*e.left, param_sizes);
        int r = estimate_expr_string_size(*e.right, param_sizes);
        return l + r;
    }

    case ExprType::Call:
        // Built-in string functions: conservative upper bounds
        switch (e.func) {
        case TokenType::KeywordLeftDollar:
        case TokenType::KeywordMidDollar:
        case TokenType::KeywordRightDollar:
            if (!e.args.empty()) {
                return estimate_expr_string_size(*e.args[0], param_sizes);
            }
            return 0;
        case TokenType::KeywordStrDollar:
            return 1 + 5 + 1; // -32768 + space
        case TokenType::KeywordChrDollar:
            return 1;
        default:
            return 0;
        }

    default:
        return 0;
    }
}

static int compute_func_return_string_size(const FuncDecl& func,
        const std::vector<std::unique_ptr<Expr>>& call_args) {
    // Build a map from parameter name -> call-site string_size
    std::unordered_map<std::string, int> param_sizes;
    for (size_t i = 0; i < func.params.size() && i < call_args.size(); ++i) {
        if (func.params[i].type == ValueType::String) {
            param_sizes[func.params[i].name] = call_args[i]->string_size;
        }
    }

    // Walk the body looking for assignments to the function name
    int max_size = 0;
    std::function<void(const StmtList&)> walk = [&](const StmtList & stmts) {
        for (const auto& stmt : stmts.statements) {
            switch (stmt->type) {
            case StmtType::Let:
                if (stmt->let_stmt->type == LetType::String &&
                        stmt->let_stmt->var == func.name) {
                    int s = estimate_expr_string_size(stmt->let_stmt->expr, param_sizes);
                    if (s > max_size) {
                        max_size = s;
                    }
                }
                break;
            case StmtType::If:
                walk(stmt->if_stmt->then_block);
                walk(stmt->if_stmt->else_block);
                break;
            case StmtType::While:
                walk(stmt->while_stmt->body);
                break;
            case StmtType::For:
                walk(stmt->for_stmt->body);
                break;
            default:
                break;
            }
        }
    };
    walk(func.body);
    return max_size;
}

void semantic_check_in_expr(Expr& e, SymbolTable& sym, const Program& prog) {
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
        if (is_string) {
            e.string_size = symbol->array_size;
        }
        break;
    }
    case ExprType::BinOp: {
        semantic_check_in_expr(*e.left, sym, prog);
        semantic_check_in_expr(*e.right, sym, prog);

        // the other operators can only be applied to ints
        if (e.left->value_type == ValueType::String ||
                e.right->value_type == ValueType::String) {
            error_at(
                e.loc,
                "operator '" + token_type_to_string(e.op) +
                "' cannot be applied to strings");
        }

        e.value_type = ValueType::Int;  // result of comparison is always int
        break;
    }
    case ExprType::RelOp: {
        semantic_check_in_expr(*e.left, sym, prog);
        semantic_check_in_expr(*e.right, sym, prog);
        if (e.left->value_type != e.right->value_type) {
            error_at(
                e.loc,
                "operator '" + token_type_to_string(e.op) +
                "' cannot be applied to different types");
        }
        e.operand_type = e.left->value_type;
        e.value_type = ValueType::Int;  // result of comparison is always int
        break;
    }
    case ExprType::UnaryOp: {
        semantic_check_in_expr(*e.inner, sym, prog);
        if (e.inner->value_type == ValueType::String) {
            error_at(
                e.loc,
                "operator '" + token_type_to_string(e.op) +
                "' cannot be applied to strings");
        }

        e.value_type = ValueType::Int;
        break;
    }
    case ExprType::ArrayAccess: {
        // assertions for errors that should have been caught in collect_symbols
        Symbol* symbol = sym.get(e.name);
        assert(symbol);
        bool is_string = is_string_var(e.name);
        assert(!is_string);
        assert(symbol->type == SymbolType::IntArrayVar);

        e.value_type = ValueType::Int;

        semantic_check_in_expr(*e.index, sym, prog);
        break;
    }
    case ExprType::Concat:
        semantic_check_in_expr(*e.left, sym, prog);
        semantic_check_in_expr(*e.right, sym, prog);
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
            semantic_check_in_expr(*arg, sym, prog);
        }

        // User-defined function call
        if (e.func == TokenType::Identifier) {
            auto it = prog.functions.find(e.name);
            if (it == prog.functions.end()) {
                error_at(e.loc,
                         "undefined function '" + e.name + "'");
            }

            const FuncDecl& func = *it->second;

            // Check argument count
            if (e.args.size() != func.params.size()) {
                error_at(e.loc,
                         "function '" + func.name + "' expects " +
                         std::to_string(func.params.size()) + " argument(s), got " +
                         std::to_string(e.args.size()));
            }

            // Check argument types
            for (size_t i = 0; i < e.args.size() && i < func.params.size(); ++i) {
                ValueType arg_type = e.args[i]->value_type;
                ValueType param_type = func.params[i].type;
                if (arg_type != param_type) {
                    std::string expected = (param_type == ValueType::String)
                                           ? "string" : "integer";
                    std::string got = (arg_type == ValueType::String)
                                      ? "string" : "integer";
                    error_at(e.args[i]->loc,
                             "argument " + std::to_string(i + 1) + " of '" +
                             func.name + "' expects " + expected +
                             " but got " + got);
                }
            }

            e.value_type = func.return_type;

            // Compute string_size for string-returning functions
            if (func.return_type == ValueType::String) {
                e.string_size = compute_func_return_string_size(func, e.args);
                if (e.string_size <= 0) {
                    e.string_size = 1; // minimum fallback
                }
            }
            break;
        }

        // Built-in function call
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

void semantic_check(StmtList& stmts, SymbolTable& sym, const Program& prog) {
    for (auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Let: {
            if (stmt->let_stmt->index) {
                semantic_check_in_expr(*stmt->let_stmt->index, sym, prog);
            }
            semantic_check_in_expr(stmt->let_stmt->expr, sym, prog);

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
            for (auto& elem : stmt->dim_stmt->elems) {
                semantic_check_in_expr(*elem.size_expr, sym, prog);
            }
            break;

        case StmtType::Input:
            break;

        case StmtType::Print:
            for (auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    semantic_check_in_expr(item.expr, sym, prog);
                }
            }
            break;

        case StmtType::If:
            semantic_check_in_expr(stmt->if_stmt->condition, sym, prog);
            semantic_check(stmt->if_stmt->then_block, sym, prog);
            semantic_check(stmt->if_stmt->else_block, sym, prog);
            break;

        case StmtType::While:
            semantic_check_in_expr(stmt->while_stmt->condition, sym, prog);
            semantic_check(stmt->while_stmt->body, sym, prog);
            break;

        case StmtType::For:
            semantic_check_in_expr(stmt->for_stmt->start_expr, sym, prog);
            semantic_check_in_expr(stmt->for_stmt->end_expr, sym, prog);
            semantic_check_in_expr(stmt->for_stmt->step_expr, sym, prog);
            semantic_check(stmt->for_stmt->body, sym, prog);
            break;

        case StmtType::Call: {
            // Check that subroutine exists
            auto it = prog.subroutines.find(stmt->call_stmt->name);
            if (it == prog.subroutines.end()) {
                error_at(stmt->loc,
                         "undefined subroutine '" + stmt->call_stmt->name + "'");
            }

            const SubDecl& sub = *it->second;

            // Check argument count matches parameter count
            if (stmt->call_stmt->args.size() != sub.params.size()) {
                error_at(stmt->loc,
                         "subroutine '" + sub.name + "' expects " +
                         std::to_string(sub.params.size()) + " argument(s), got " +
                         std::to_string(stmt->call_stmt->args.size()));
            }

            // Semantic-check each argument expression and verify type match
            for (size_t i = 0; i < stmt->call_stmt->args.size(); ++i) {
                semantic_check_in_expr(stmt->call_stmt->args[i], sym, prog);

                if (i < sub.params.size()) {
                    ValueType arg_type = stmt->call_stmt->args[i].value_type;
                    ValueType param_type = sub.params[i].type;
                    if (arg_type != param_type) {
                        std::string expected = (param_type == ValueType::String)
                                               ? "string" : "integer";
                        std::string got = (arg_type == ValueType::String)
                                          ? "string" : "integer";
                        error_at(stmt->call_stmt->args[i].loc,
                                 "argument " + std::to_string(i + 1) + " of '" +
                                 sub.name + "' expects " + expected +
                                 " but got " + got);
                    }
                }
            }
            break;
        }
        case StmtType::SubDecl:
            break;

        default:
            assert(0);
        }
    }
}

// Check that a subroutine body does not call itself (recursion not supported)
// and does not assign to its own name
void check_sub_body_no_recursion(const StmtList& stmts,
                                 const std::string& sub_name) {
    for (const auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Call:
            if (stmt->call_stmt->name == sub_name) {
                error_at(stmt->loc,
                         "subroutine '" + sub_name + "' cannot call itself (recursion is not supported)");
            }
            break;

        case StmtType::Let:
            if ((stmt->let_stmt->type == LetType::Normal ||
                    stmt->let_stmt->type == LetType::String) &&
                    stmt->let_stmt->var == sub_name) {
                error_at(stmt->loc,
                         "cannot assign to subroutine name '" + sub_name + "' (subroutines return void)");
            }
            break;

        case StmtType::If:
            check_sub_body_no_recursion(stmt->if_stmt->then_block, sub_name);
            check_sub_body_no_recursion(stmt->if_stmt->else_block, sub_name);
            break;

        case StmtType::While:
            check_sub_body_no_recursion(stmt->while_stmt->body, sub_name);
            break;

        case StmtType::For:
            if (stmt->for_stmt->var == sub_name) {
                error_at(stmt->loc,
                         "cannot assign to subroutine name '" + sub_name + "' (subroutines return void)");
            }
            check_sub_body_no_recursion(stmt->for_stmt->body, sub_name);
            break;

        case StmtType::Input:
            for (const auto& var : stmt->input_stmt->vars) {
                if (var == sub_name) {
                    error_at(stmt->loc,
                             "cannot assign to subroutine name '" + sub_name + "' (subroutines return void)");
                }
            }
            break;

        default:
            break;
        }
    }
}

// Check that a function body does not call itself (recursion not supported)
// and contains at least one assignment to the function name
static void check_func_body_no_recursion_in_expr(const Expr& e,
        const std::string& func_name) {
    switch (e.expr_type) {
    case ExprType::Call:
        if (e.func == TokenType::Identifier && e.name == func_name) {
            error_at(e.loc,
                     "function '" + func_name + "' cannot call itself (recursion is not supported)");
        }
        for (const auto& arg : e.args) {
            check_func_body_no_recursion_in_expr(*arg, func_name);
        }
        break;
    case ExprType::BinOp:
    case ExprType::RelOp:
    case ExprType::Concat:
        check_func_body_no_recursion_in_expr(*e.left, func_name);
        check_func_body_no_recursion_in_expr(*e.right, func_name);
        break;
    case ExprType::UnaryOp:
        check_func_body_no_recursion_in_expr(*e.inner, func_name);
        break;
    case ExprType::ArrayAccess:
        if (e.name == func_name) {
            error_at(e.loc,
                     "function '" + func_name + "' cannot call itself (recursion is not supported)");
        }
        check_func_body_no_recursion_in_expr(*e.index, func_name);
        break;
    default:
        break;
    }
}

static void check_func_body_no_recursion(const StmtList& stmts,
        const std::string& func_name) {
    for (const auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            if (stmt->let_stmt->index) {
                check_func_body_no_recursion_in_expr(*stmt->let_stmt->index, func_name);
            }
            check_func_body_no_recursion_in_expr(stmt->let_stmt->expr, func_name);
            break;
        case StmtType::Dim:
            for (const auto& elem : stmt->dim_stmt->elems) {
                check_func_body_no_recursion_in_expr(*elem.size_expr, func_name);
            }
            break;
        case StmtType::Print:
            for (const auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    check_func_body_no_recursion_in_expr(item.expr, func_name);
                }
            }
            break;
        case StmtType::If:
            check_func_body_no_recursion_in_expr(stmt->if_stmt->condition, func_name);
            check_func_body_no_recursion(stmt->if_stmt->then_block, func_name);
            check_func_body_no_recursion(stmt->if_stmt->else_block, func_name);
            break;
        case StmtType::While:
            check_func_body_no_recursion_in_expr(stmt->while_stmt->condition, func_name);
            check_func_body_no_recursion(stmt->while_stmt->body, func_name);
            break;
        case StmtType::For:
            check_func_body_no_recursion_in_expr(stmt->for_stmt->start_expr, func_name);
            check_func_body_no_recursion_in_expr(stmt->for_stmt->end_expr, func_name);
            check_func_body_no_recursion_in_expr(stmt->for_stmt->step_expr, func_name);
            check_func_body_no_recursion(stmt->for_stmt->body, func_name);
            break;
        case StmtType::Call:
            for (const auto& arg : stmt->call_stmt->args) {
                check_func_body_no_recursion_in_expr(arg, func_name);
            }
            break;
        default:
            break;
        }
    }
}

// Check that the function body assigns to the function name at least once
static bool has_return_assignment(const StmtList& stmts,
                                  const std::string& func_name) {
    for (const auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            if ((stmt->let_stmt->type == LetType::Normal ||
                    stmt->let_stmt->type == LetType::String) &&
                    stmt->let_stmt->var == func_name) {
                return true;
            }
            break;
        case StmtType::If:
            if (has_return_assignment(stmt->if_stmt->then_block, func_name) ||
                    has_return_assignment(stmt->if_stmt->else_block, func_name)) {
                return true;
            }
            break;
        case StmtType::While:
            if (has_return_assignment(stmt->while_stmt->body, func_name)) {
                return true;
            }
            break;
        case StmtType::For:
            if (has_return_assignment(stmt->for_stmt->body, func_name)) {
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}

void finalize_array_sizes(StmtList& stmts, SymbolTable& sym) {
    for (auto& stmt : stmts.statements) {
        if (stmt->type != StmtType::Dim) {
            continue;
        }

        for (auto& elem : stmt->dim_stmt->elems) {
            // After fold_constants, size_expr must be a number
            auto folded = fold_const_int_expr(*elem.size_expr);
            if (!folded) {
                error_at(stmt->loc, "DIM size must be a constant expression");
            }
            int size = *folded;
            if (size <= 0) {
                error_at(stmt->loc, "DIM size must be > 0");
            }

            Symbol* symbol = sym.get(elem.var);
            assert(symbol);
            symbol->array_size = size;
        }
    }
}

// helper to count discovered constants
static std::size_t count_constant_symbols(const SymbolTable& sym) {
    std::size_t n = 0;
    for (const auto& kv : sym.all()) {
        if (kv.second.const_value || kv.second.const_string) {
            ++n;
        }
    }
    return n;
}

//-----------------------------------------------------------------------------
// Subroutine and function inlining
//-----------------------------------------------------------------------------

// Collect all identifiers defined (assigned/declared) inside a SubDecl body.
// These are the "local" variables that need renaming during inlining.
static void collect_local_vars_in_expr(const Expr& e,
                                       std::unordered_set<std::string>& locals) {
    switch (e.expr_type) {
    case ExprType::Var:
        locals.insert(e.name);
        break;
    case ExprType::BinOp:
    case ExprType::RelOp:
    case ExprType::Concat:
        collect_local_vars_in_expr(*e.left, locals);
        collect_local_vars_in_expr(*e.right, locals);
        break;
    case ExprType::UnaryOp:
        collect_local_vars_in_expr(*e.inner, locals);
        break;
    case ExprType::ArrayAccess:
        locals.insert(e.name);
        collect_local_vars_in_expr(*e.index, locals);
        break;
    case ExprType::Call:
        for (const auto& arg : e.args) {
            collect_local_vars_in_expr(*arg, locals);
        }
        break;
    case ExprType::Number:
    case ExprType::StringLiteral:
        break;
    default:
        break;
    }
}

static void collect_local_vars(const StmtList& stmts,
                               std::unordered_set<std::string>& locals) {
    for (const auto& stmt : stmts.statements) {
        switch (stmt->type) {
        case StmtType::Let:
            locals.insert(stmt->let_stmt->var);
            if (stmt->let_stmt->index) {
                collect_local_vars_in_expr(*stmt->let_stmt->index, locals);
            }
            collect_local_vars_in_expr(stmt->let_stmt->expr, locals);
            break;
        case StmtType::Dim:
            for (const auto& elem : stmt->dim_stmt->elems) {
                locals.insert(elem.var);
                collect_local_vars_in_expr(*elem.size_expr, locals);
            }
            break;
        case StmtType::Input:
            for (const auto& v : stmt->input_stmt->vars) {
                locals.insert(v);
            }
            break;
        case StmtType::Print:
            for (const auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    collect_local_vars_in_expr(item.expr, locals);
                }
            }
            break;
        case StmtType::If:
            collect_local_vars_in_expr(stmt->if_stmt->condition, locals);
            collect_local_vars(stmt->if_stmt->then_block, locals);
            collect_local_vars(stmt->if_stmt->else_block, locals);
            break;
        case StmtType::While:
            collect_local_vars_in_expr(stmt->while_stmt->condition, locals);
            collect_local_vars(stmt->while_stmt->body, locals);
            break;
        case StmtType::For:
            locals.insert(stmt->for_stmt->var);
            collect_local_vars_in_expr(stmt->for_stmt->start_expr, locals);
            collect_local_vars_in_expr(stmt->for_stmt->end_expr, locals);
            collect_local_vars_in_expr(stmt->for_stmt->step_expr, locals);
            collect_local_vars(stmt->for_stmt->body, locals);
            break;
        case StmtType::Call:
            for (const auto& arg : stmt->call_stmt->args) {
                collect_local_vars_in_expr(arg, locals);
            }
            break;
        default:
            break;
        }
    }
}

// Build a rename map: for each local variable, map to <sub>_<n>_<var>
// The $ suffix must be preserved for string variables.
using RenameMap = std::unordered_map<std::string, std::string>;

static RenameMap build_rename_map(const std::string& decl_name,
                                  const std::vector<Param>& params,
                                  const std::unordered_set<std::string>& locals,
                                  int instance,
                                  const SymbolTable& sym) {
    RenameMap map;
    std::string prefix = decl_name + "_" + std::to_string(instance) + "_";

    // Parameters always get renamed
    for (const auto& p : params) {
        map[p.name] = prefix + p.name;
    }

    // The function return variable (decl_name itself) gets renamed too
    if (!map.count(decl_name)) {
        map[decl_name] = prefix + decl_name;
    }

    // Local variables (not already in symbol table = not global) get renamed
    for (const auto& var : locals) {
        if (map.count(var)) {
            continue; // already a parameter or return var
        }
        if (sym.get(var) != nullptr) {
            continue; // global variable, don't rename
        }
        map[var] = prefix + var;
    }

    return map;
}

static std::string rename_var(const std::string& name, const RenameMap& map) {
    auto it = map.find(name);
    if (it != map.end()) {
        return it->second;
    }
    return name;
}

// Deep copy an expression, renaming variables according to the map
static Expr deep_copy_expr(const Expr& e, const RenameMap& map);

static std::unique_ptr<Expr> deep_copy_expr_ptr(const Expr& e,
        const RenameMap& map) {
    return std::make_unique<Expr>(deep_copy_expr(e, map));
}

static Expr deep_copy_expr(const Expr& e, const RenameMap& map) {
    Expr c;
    c.expr_type = e.expr_type;
    c.value_type = e.value_type;
    c.operand_type = e.operand_type;
    c.loc = e.loc;
    c.int_value = e.int_value;
    c.string_value = e.string_value;
    c.string_size = e.string_size;
    c.op = e.op;
    c.func = e.func;

    switch (e.expr_type) {
    case ExprType::Number:
    case ExprType::StringLiteral:
        break;

    case ExprType::Var:
        c.name = rename_var(e.name, map);
        c.value_type = is_string_var(c.name) ? ValueType::String : ValueType::Int;
        break;

    case ExprType::BinOp:
    case ExprType::RelOp:
    case ExprType::Concat:
        c.left = deep_copy_expr_ptr(*e.left, map);
        c.right = deep_copy_expr_ptr(*e.right, map);
        break;

    case ExprType::UnaryOp:
        c.inner = deep_copy_expr_ptr(*e.inner, map);
        break;

    case ExprType::ArrayAccess:
        c.name = rename_var(e.name, map);
        c.index = deep_copy_expr_ptr(*e.index, map);
        break;

    case ExprType::Call:
        c.name = e.name; // don't rename built-in or user function names
        for (const auto& arg : e.args) {
            c.args.push_back(deep_copy_expr_ptr(*arg, map));
        }
        break;

    default:
        assert(0);
    }

    return c;
}

// Deep copy a statement list, renaming variables
static void deep_copy_stmts(const StmtList& src, const RenameMap& map,
                            std::vector<std::unique_ptr<Stmt>>& out);

static std::unique_ptr<Stmt> deep_copy_stmt(const Stmt& s,
        const RenameMap& map) {
    auto c = std::make_unique<Stmt>();
    c->type = s.type;
    c->loc = s.loc;

    switch (s.type) {
    case StmtType::Let: {
        c->let_stmt = std::make_unique<LetStmt>();
        c->let_stmt->var = rename_var(s.let_stmt->var, map);
        c->let_stmt->type = s.let_stmt->type;
        if (s.let_stmt->index) {
            c->let_stmt->index = deep_copy_expr_ptr(*s.let_stmt->index, map);
        }
        c->let_stmt->expr = deep_copy_expr(s.let_stmt->expr, map);
        break;
    }
    case StmtType::Dim: {
        c->dim_stmt = std::make_unique<DimStmt>();
        for (const auto& elem : s.dim_stmt->elems) {
            DimElem ce;
            ce.var = rename_var(elem.var, map);
            ce.size_expr = deep_copy_expr_ptr(*elem.size_expr, map);
            c->dim_stmt->elems.push_back(std::move(ce));
        }
        break;
    }
    case StmtType::Input: {
        c->input_stmt = std::make_unique<InputStmt>();
        for (const auto& v : s.input_stmt->vars) {
            c->input_stmt->vars.push_back(rename_var(v, map));
        }
        break;
    }
    case StmtType::Print: {
        c->print_stmt = std::make_unique<PrintStmt>();
        for (const auto& item : s.print_stmt->elems) {
            PrintElem pe;
            pe.type = item.type;
            pe.text = item.text;
            pe.sep = item.sep;
            if (item.type == PrintElemType::Expr) {
                pe.expr = deep_copy_expr(item.expr, map);
            }
            c->print_stmt->elems.push_back(std::move(pe));
        }
        break;
    }
    case StmtType::If: {
        c->if_stmt = std::make_unique<IfStmt>();
        c->if_stmt->condition = deep_copy_expr(s.if_stmt->condition, map);
        deep_copy_stmts(s.if_stmt->then_block, map,
                        c->if_stmt->then_block.statements);
        deep_copy_stmts(s.if_stmt->else_block, map,
                        c->if_stmt->else_block.statements);
        break;
    }
    case StmtType::While: {
        c->while_stmt = std::make_unique<WhileStmt>();
        c->while_stmt->condition = deep_copy_expr(s.while_stmt->condition, map);
        deep_copy_stmts(s.while_stmt->body, map,
                        c->while_stmt->body.statements);
        break;
    }
    case StmtType::For: {
        c->for_stmt = std::make_unique<ForStmt>();
        c->for_stmt->var = rename_var(s.for_stmt->var, map);
        c->for_stmt->start_expr = deep_copy_expr(s.for_stmt->start_expr, map);
        c->for_stmt->end_expr = deep_copy_expr(s.for_stmt->end_expr, map);
        c->for_stmt->step_expr = deep_copy_expr(s.for_stmt->step_expr, map);
        deep_copy_stmts(s.for_stmt->body, map,
                        c->for_stmt->body.statements);
        break;
    }
    case StmtType::Call: {
        c->call_stmt = std::make_unique<CallStmt>();
        c->call_stmt->name = s.call_stmt->name;
        for (const auto& arg : s.call_stmt->args) {
            c->call_stmt->args.push_back(deep_copy_expr(arg, map));
        }
        break;
    }
    case StmtType::SubDecl:
        break;

    default:
        assert(0);
    }

    return c;
}

static void deep_copy_stmts(const StmtList& src, const RenameMap& map,
                            std::vector<std::unique_ptr<Stmt>>& out) {
    for (const auto& s : src.statements) {
        out.push_back(deep_copy_stmt(*s, map));
    }
}

// Helper: emit parameter setup stmts (DIM for strings, LET for assignment)
// into new_stmts. Shared between SUB and FUNCTION inlining.
static void emit_param_setup(const std::vector<Param>& params,
                             const std::vector<Expr>& call_args,
                             const std::vector<std::unique_ptr<Expr>>& call_expr_args,
                             const RenameMap& map,
                             const SourceLoc& loc,
                             std::vector<std::unique_ptr<Stmt>>& new_stmts) {
    // Determine which arg list to use (statement Call uses vector<Expr>,
    // expression Call uses vector<unique_ptr<Expr>>)
    size_t n = params.size();
    for (size_t i = 0; i < n; ++i) {
        bool is_str = (params[i].type == ValueType::String);
        std::string renamed = rename_var(params[i].name, map);

        const Expr& arg = call_args.empty()
                          ? *call_expr_args[i]
                          : call_args[i];

        if (is_str) {
            int arg_size = arg.string_size;
            if (arg_size <= 0) {
                arg_size = 1;
            }

            auto dim = std::make_unique<DimStmt>();
            DimElem elem;
            elem.var = renamed;
            elem.size_expr = std::make_unique<Expr>(Expr::number(arg_size, loc));
            dim->elems.push_back(std::move(elem));

            auto ds = std::make_unique<Stmt>();
            ds->type = StmtType::Dim;
            ds->loc = loc;
            ds->dim_stmt = std::move(dim);
            new_stmts.push_back(std::move(ds));
        }

        auto let = std::make_unique<LetStmt>();
        let->var = renamed;
        let->type = is_str ? LetType::String : LetType::Normal;
        RenameMap empty_map;
        let->expr = deep_copy_expr(arg, empty_map);

        auto s = std::make_unique<Stmt>();
        s->type = StmtType::Let;
        s->loc = loc;
        s->let_stmt = std::move(let);
        new_stmts.push_back(std::move(s));
    }
}

// Inline all CALL statements in a StmtList.
// Returns true if any inlining was performed.
static bool inline_calls(StmtList& stmts, const Program& prog,
                         const SymbolTable& sym);

// Inline user-defined function calls inside an expression.
// Emits the function body as statements before the expression,
// and replaces the Call node with a Var referencing the renamed return variable.
// Returns true if any inlining was performed.
static bool inline_func_calls_in_expr(Expr& e, const Program& prog,
                                      const SymbolTable& sym,
                                      std::vector<std::unique_ptr<Stmt>>& pre_stmts) {
    bool changed = false;

    switch (e.expr_type) {
    case ExprType::Number:
    case ExprType::StringLiteral:
    case ExprType::Var:
        break;

    case ExprType::BinOp:
    case ExprType::RelOp:
    case ExprType::Concat:
        if (inline_func_calls_in_expr(*e.left, prog, sym, pre_stmts)) {
            changed = true;
        }
        if (inline_func_calls_in_expr(*e.right, prog, sym, pre_stmts)) {
            changed = true;
        }
        break;

    case ExprType::UnaryOp:
        if (inline_func_calls_in_expr(*e.inner, prog, sym, pre_stmts)) {
            changed = true;
        }
        break;

    case ExprType::ArrayAccess:
        if (inline_func_calls_in_expr(*e.index, prog, sym, pre_stmts)) {
            changed = true;
        }
        break;

    case ExprType::Call:
        // First recurse into arguments
        for (auto& arg : e.args) {
            if (inline_func_calls_in_expr(*arg, prog, sym, pre_stmts)) {
                changed = true;
            }
        }

        // If this is a user-defined function call, inline it
        if (e.func == TokenType::Identifier) {
            auto it = prog.functions.find(e.name);
            if (it != prog.functions.end()) {
                changed = true;
                const FuncDecl& func = *it->second;
                int instance = ++g_inline_counter;

                // Collect local vars in function body
                std::unordered_set<std::string> locals;
                for (const auto& p : func.params) {
                    locals.insert(p.name);
                }
                locals.insert(func.name); // return variable
                collect_local_vars(func.body, locals);

                // Build rename map
                RenameMap map = build_rename_map(
                                    func.name, func.params, locals, instance, sym);

                std::string renamed_return = rename_var(func.name, map);

                // Emit DIM for string return variable
                if (func.return_type == ValueType::String) {
                    auto dim = std::make_unique<DimStmt>();
                    DimElem elem;
                    elem.var = renamed_return;
                    int ret_size = e.string_size > 0 ? e.string_size : 1;
                    elem.size_expr = std::make_unique<Expr>(
                                         Expr::number(ret_size, e.loc));
                    dim->elems.push_back(std::move(elem));

                    auto ds = std::make_unique<Stmt>();
                    ds->type = StmtType::Dim;
                    ds->loc = e.loc;
                    ds->dim_stmt = std::move(dim);
                    pre_stmts.push_back(std::move(ds));
                }

                // Emit parameter setup
                std::vector<Expr> empty_args;
                emit_param_setup(func.params, empty_args, e.args,
                                 map, e.loc, pre_stmts);

                // Deep-copy function body with renaming
                deep_copy_stmts(func.body, map, pre_stmts);

                // Replace this Call expression with Var(renamed_return)
                e = Expr::var(renamed_return, e.loc);
            }
        }
        break;

    default:
        break;
    }

    return changed;
}

// Walk all expressions in a statement list and inline function calls.
// Returns true if any inlining was performed.
static bool inline_func_calls(StmtList& stmts, const Program& prog,
                              const SymbolTable& sym) {
    bool changed = false;
    std::vector<std::unique_ptr<Stmt>> new_stmts;

    for (auto& stmt : stmts.statements) {
        // Recurse into nested blocks
        switch (stmt->type) {
        case StmtType::If:
            if (inline_func_calls(stmt->if_stmt->then_block, prog, sym)) {
                changed = true;
            }
            if (inline_func_calls(stmt->if_stmt->else_block, prog, sym)) {
                changed = true;
            }
            break;
        case StmtType::While:
            if (inline_func_calls(stmt->while_stmt->body, prog, sym)) {
                changed = true;
            }
            break;
        case StmtType::For:
            if (inline_func_calls(stmt->for_stmt->body, prog, sym)) {
                changed = true;
            }
            break;
        default:
            break;
        }

        // Collect pre-statements from inlining function calls in expressions
        std::vector<std::unique_ptr<Stmt>> pre_stmts;

        switch (stmt->type) {
        case StmtType::Let:
            if (stmt->let_stmt->index) {
                if (inline_func_calls_in_expr(*stmt->let_stmt->index, prog, sym, pre_stmts)) {
                    changed = true;
                }
            }
            if (inline_func_calls_in_expr(stmt->let_stmt->expr, prog, sym, pre_stmts)) {
                changed = true;
            }
            break;

        case StmtType::Dim:
            for (auto& elem : stmt->dim_stmt->elems) {
                if (inline_func_calls_in_expr(*elem.size_expr, prog, sym, pre_stmts)) {
                    changed = true;
                }
            }
            break;

        case StmtType::Print:
            for (auto& item : stmt->print_stmt->elems) {
                if (item.type == PrintElemType::Expr) {
                    if (inline_func_calls_in_expr(item.expr, prog, sym, pre_stmts)) {
                        changed = true;
                    }
                }
            }
            break;

        case StmtType::If:
            if (inline_func_calls_in_expr(stmt->if_stmt->condition, prog, sym, pre_stmts)) {
                changed = true;
            }
            break;

        case StmtType::While:
            if (inline_func_calls_in_expr(stmt->while_stmt->condition, prog, sym, pre_stmts)) {
                changed = true;
            }
            break;

        case StmtType::For:
            if (inline_func_calls_in_expr(stmt->for_stmt->start_expr, prog, sym, pre_stmts)) {
                changed = true;
            }
            if (inline_func_calls_in_expr(stmt->for_stmt->end_expr, prog, sym, pre_stmts)) {
                changed = true;
            }
            if (inline_func_calls_in_expr(stmt->for_stmt->step_expr, prog, sym, pre_stmts)) {
                changed = true;
            }
            break;

        case StmtType::Call:
            for (auto& arg : stmt->call_stmt->args) {
                if (inline_func_calls_in_expr(arg, prog, sym, pre_stmts)) {
                    changed = true;
                }
            }
            break;

        default:
            break;
        }

        // Emit pre-statements, then the original statement
        for (auto& ps : pre_stmts) {
            new_stmts.push_back(std::move(ps));
        }
        new_stmts.push_back(std::move(stmt));
    }

    stmts.statements = std::move(new_stmts);
    return changed;
}

// Inline all CALL statements (SUB calls) in a StmtList.
static bool inline_calls(StmtList& stmts, const Program& prog,
                         const SymbolTable& sym) {
    bool changed = false;
    std::vector<std::unique_ptr<Stmt>> new_stmts;

    for (auto& stmt : stmts.statements) {
        // Recurse into nested blocks first
        switch (stmt->type) {
        case StmtType::If:
            if (inline_calls(stmt->if_stmt->then_block, prog, sym)) {
                changed = true;
            }
            if (inline_calls(stmt->if_stmt->else_block, prog, sym)) {
                changed = true;
            }
            break;
        case StmtType::While:
            if (inline_calls(stmt->while_stmt->body, prog, sym)) {
                changed = true;
            }
            break;
        case StmtType::For:
            if (inline_calls(stmt->for_stmt->body, prog, sym)) {
                changed = true;
            }
            break;
        default:
            break;
        }

        if (stmt->type != StmtType::Call) {
            new_stmts.push_back(std::move(stmt));
            continue;
        }

        // Inline this call
        changed = true;
        const CallStmt& call = *stmt->call_stmt;
        auto it = prog.subroutines.find(call.name);
        assert(it != prog.subroutines.end());
        const SubDecl& sub = *it->second;

        int instance = ++g_inline_counter;

        // Collect all variables referenced in the sub body
        std::unordered_set<std::string> locals;
        for (const auto& p : sub.params) {
            locals.insert(p.name);
        }
        collect_local_vars(sub.body, locals);

        // Build rename map
        RenameMap map = build_rename_map(
                            sub.name, sub.params, locals, instance, sym);

        // Emit parameter setup
        std::vector<std::unique_ptr<Expr>> empty_expr_args;
        emit_param_setup(sub.params, call.args, empty_expr_args,
                         map, stmt->loc, new_stmts);

        // Deep-copy sub body with renaming
        deep_copy_stmts(sub.body, map, new_stmts);
    }

    stmts.statements = std::move(new_stmts);
    return changed;
}

//-----------------------------------------------------------------------------

std::string compile(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    Program prog = parser.parse_program();

    // Reclassify single-arg ArrayAccess nodes that are function calls
    reclassify_func_calls(prog.stmts, prog);
    for (auto& [name, func] : prog.functions) {
        reclassify_func_calls(func->body, prog);
    }
    for (auto& [name, sub] : prog.subroutines) {
        reclassify_func_calls(sub->body, prog);
    }

    // Validate subroutine bodies for structural constraints only
    // (no recursion, no assignment to sub name).
    for (auto& [name, sub] : prog.subroutines) {
        check_sub_body_no_recursion(sub->body, name);
    }

    // Validate function bodies (no recursion, has return assignment)
    for (auto& [name, func] : prog.functions) {
        check_func_body_no_recursion(func->body, name);
        if (!has_return_assignment(func->body, name)) {
            error_at(func->body.statements.empty()
                     ? SourceLoc(0) : func->body.statements.front()->loc,
                     "function '" + name + "' must assign a return value to '" + name + "'");
        }
    }

    SymbolTable sym;
    bool inlined;

    do {
        sym.clear();
        collect_symbols(prog.stmts, sym, prog);

        // propagate single-assignment constants to a fixed point
        std::size_t last_const = 0;
        while (true) {
            mark_single_assignment_constants(prog.stmts, sym);
            fold_constants(prog.stmts, sym);
            std::size_t now_const = count_constant_symbols(sym);
            if (now_const == last_const) {
                break;
            }
            last_const = now_const;
        }

        finalize_array_sizes(prog.stmts, sym);
        semantic_check(prog.stmts, sym, prog);

        // Inline function calls in expressions first, then SUB calls
        g_inline_counter = 0;
        bool func_inlined = inline_func_calls(prog.stmts, prog, sym);
        bool sub_inlined = inline_calls(prog.stmts, prog, sym);
        inlined = func_inlined || sub_inlined;
    }
    while (inlined);

    CodeGen cg(sym);
    return cg.generate(prog.stmts);
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
