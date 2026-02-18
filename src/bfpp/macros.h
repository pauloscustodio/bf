//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "files.h"
#include "lexer.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Macro {
    std::string name;
    std::vector<std::string> params;
    std::vector<Token> body;
    SourceLocation loc;
};

class MacroTable {
public:
    MacroTable() = default;

    void clear();
    bool define(const Macro& macro);
    void undef(const std::string& name);
    const Macro* lookup(const std::string& name) const;

private:
    std::unordered_map<std::string, Macro> table_;
};

extern MacroTable g_macro_table;

// forward declarations
class Parser;
struct Array;

enum class BuiltinStruct {
    NONE,
    IF,
    ELSE,
    WHILE,
    REPEAT,
};

struct BuiltinStructLevel {
    BuiltinStruct type = BuiltinStruct::NONE;
    SourceLocation loc;
    std::string temp_if;
    std::string temp_else;
    int cond = 0;
};

class MacroExpander {
public:
    MacroExpander(MacroTable& table, Parser* parser);

    // Returns true if token was expanded, false otherwise
    bool try_expand(Parser& parser, const Token& token);
    void remove_expanding(const std::string& name);

    // Collect arguments for a macro call.
    // Returns false on syntax error (error already reported), true otherwise.
    bool collect_args(Parser& parser,
                      const Macro& macro,
                      std::vector<std::vector<Token>>& args);

    // Query built-in names (needed by is_reserved_keyword, etc.)
    static bool is_builtin_name(const std::string& name);

    // check if the struct stack is empty
    void check_struct_stack() const;

private:
    using BuiltinHandler = bool (MacroExpander::*)(Parser& parser,
                           const Token& tok);
    static const std::unordered_map<std::string, BuiltinHandler> kBuiltins;

    MacroTable& table_;
    Parser* parser_ = nullptr;
    std::unordered_set<std::string> expanding_; // recursion guard
    std::vector<BuiltinStructLevel> struct_stack_;

    bool handle_alloc_cell8(Parser& parser, const Token& tok);
    bool handle_alloc_cell16(Parser& parser, const Token& tok);
    bool handle_free_cell8(Parser& parser, const Token& tok);
    bool handle_free_cell16(Parser& parser, const Token& tok);
    bool handle_clear8(Parser& parser, const Token& tok);
    bool handle_clear16(Parser& parser, const Token& tok);
    bool handle_set8(Parser& parser, const Token& tok);
    bool handle_set16(Parser& parser, const Token& tok);
    bool handle_move8(Parser& parser, const Token& tok);
    bool handle_move16(Parser& parser, const Token& tok);
    bool handle_copy8(Parser& parser, const Token& tok);
    bool handle_copy16(Parser& parser, const Token& tok);
    bool handle_not8(Parser& parser, const Token& tok);
    bool handle_not16(Parser& parser, const Token& tok);
    bool handle_and8(Parser& parser, const Token& tok);
    bool handle_and16(Parser& parser, const Token& tok);
    bool handle_or8(Parser& parser, const Token& tok);
    bool handle_or16(Parser& parser, const Token& tok);
    bool handle_xor8(Parser& parser, const Token& tok);
    bool handle_xor16(Parser& parser, const Token& tok);
    bool handle_add8(Parser& parser, const Token& tok);
    bool handle_add16(Parser& parser, const Token& tok);
    bool handle_add8s(Parser& parser, const Token& tok);
    bool handle_add16s(Parser& parser, const Token& tok);
    bool handle_sub8(Parser& parser, const Token& tok);
    bool handle_sub16(Parser& parser, const Token& tok);
    bool handle_sub8s(Parser& parser, const Token& tok);
    bool handle_sub16s(Parser& parser, const Token& tok);
    bool handle_neg8(Parser& parser, const Token& tok);
    bool handle_neg16(Parser& parser, const Token& tok);
    bool handle_sign8(Parser& parser, const Token& tok);
    bool handle_sign16(Parser& parser, const Token& tok);
    bool handle_abs8(Parser& parser, const Token& tok);
    bool handle_abs16(Parser& parser, const Token& tok);
    bool handle_mul8(Parser& parser, const Token& tok);
    bool handle_mul16(Parser& parser, const Token& tok);
    bool handle_mul8s(Parser& parser, const Token& tok);
    bool handle_mul16s(Parser& parser, const Token& tok);
    bool handle_div8(Parser& parser, const Token& tok);
    bool handle_div16(Parser& parser, const Token& tok);
    bool handle_div8s(Parser& parser, const Token& tok);
    bool handle_div16s(Parser& parser, const Token& tok);
    bool handle_mod8(Parser& parser, const Token& tok);
    bool handle_mod16(Parser& parser, const Token& tok);
    bool handle_mod8s(Parser& parser, const Token& tok);
    bool handle_mod16s(Parser& parser, const Token& tok);
    bool handle_div8_mod8(Parser& parser, const Token& tok,
                          bool return_remainder);
    bool handle_div16_mod16(Parser& parser, const Token& tok,
                            bool return_remainder);
    bool handle_div8s_mod8s(Parser& parser, const Token& tok,
                            bool return_remainder);
    bool handle_div16s_mod16s(Parser& parser, const Token& tok,
                              bool return_remainder);
    bool handle_eq8(Parser& parser, const Token& tok);
    bool handle_eq16(Parser& parser, const Token& tok);
    bool handle_eq8s(Parser& parser, const Token& tok);
    bool handle_eq16s(Parser& parser, const Token& tok);
    bool handle_ne8(Parser& parser, const Token& tok);
    bool handle_ne16(Parser& parser, const Token& tok);
    bool handle_ne8s(Parser& parser, const Token& tok);
    bool handle_ne16s(Parser& parser, const Token& tok);
    bool handle_lt8(Parser& parser, const Token& tok);
    bool handle_lt16(Parser& parser, const Token& tok);
    bool handle_lt8s(Parser& parser, const Token& tok);
    bool handle_lt16s(Parser& parser, const Token& tok);
    bool handle_gt8(Parser& parser, const Token& tok);
    bool handle_gt16(Parser& parser, const Token& tok);
    bool handle_gt8s(Parser& parser, const Token& tok);
    bool handle_gt16s(Parser& parser, const Token& tok);
    bool handle_le8(Parser& parser, const Token& tok);
    bool handle_le16(Parser& parser, const Token& tok);
    bool handle_le8s(Parser& parser, const Token& tok);
    bool handle_le16s(Parser& parser, const Token& tok);
    bool handle_ge8(Parser& parser, const Token& tok);
    bool handle_ge16(Parser& parser, const Token& tok);
    bool handle_ge8s(Parser& parser, const Token& tok);
    bool handle_ge16s(Parser& parser, const Token& tok);
    bool handle_shr8(Parser& parser, const Token& tok);
    bool handle_shr16(Parser& parser, const Token& tok);
    bool handle_shl8(Parser& parser, const Token& tok);
    bool handle_shl16(Parser& parser, const Token& tok);
    bool handle_powX(Parser& parser, const Token& tok,
                     int width, bool signed_version);
    bool handle_pow8(Parser& parser, const Token& tok);
    bool handle_pow16(Parser& parser, const Token& tok);
    bool handle_pow8s(Parser& parser, const Token& tok);
    bool handle_pow16s(Parser& parser, const Token& tok);
    bool handle_if(Parser& parser, const Token& tok);
    bool handle_else(Parser& parser, const Token& tok);
    bool handle_endif(Parser& parser, const Token& tok);
    bool handle_while(Parser& parser, const Token& tok);
    bool handle_endwhile(Parser& parser, const Token& tok);
    bool handle_repeat(Parser& parser, const Token& tok);
    bool handle_endrepeat(Parser& parser, const Token& tok);
    bool handle_push8(Parser& parser, const Token& tok);
    bool handle_push16(Parser& parser, const Token& tok);
    bool handle_push8i(Parser& parser, const Token& tok);
    bool handle_push16i(Parser& parser, const Token& tok);
    bool handle_pop8(Parser& parser, const Token& tok);
    bool handle_pop16(Parser& parser, const Token& tok);
    bool handle_alloc_global16(Parser& parser, const Token& tok);
    bool handle_free_global16(Parser& parser, const Token& tok);
    bool handle_alloc_temp16(Parser& parser, const Token& tok);
    bool handle_free_temp16(Parser& parser, const Token& tok);
    bool handle_enter_frame16(Parser& parser, const Token& tok);
    bool handle_leave_frame16(Parser& parser, const Token& tok);
    bool handle_frame_alloc_temp16(Parser& parser, const Token& tok);
    bool handle_print_char(Parser& parser, const Token& tok);
    bool handle_print_char8(Parser& parser, const Token& tok);
    bool handle_print_string(Parser& parser, const Token& tok);
    bool handle_print_newline(Parser& parser, const Token& tok);
    bool handle_print_cellX(Parser& parser, const Token& tok, int width);
    bool handle_print_cell8(Parser& parser, const Token& tok);
    bool handle_print_cell16(Parser& parser, const Token& tok);
    bool handle_print_cellXs(Parser& parser, const Token& tok, int width);
    bool handle_print_cell8s(Parser& parser, const Token& tok);
    bool handle_print_cell16s(Parser& parser, const Token& tok);
    bool handle_scan_char8(Parser& parser, const Token& tok);
    bool handle_unscan_char8(Parser& parser, const Token& tok);
    bool handle_scan_spaces(Parser& parser, const Token& tok);
    bool handle_scan_cellX(Parser& parser, const Token& tok, int width);
    bool handle_scan_cell8(Parser& parser, const Token& tok);
    bool handle_scan_cell16(Parser& parser, const Token& tok);
    bool handle_scan_cellXs(Parser& parser, const Token& tok, int width);
    bool handle_scan_cell8s(Parser& parser, const Token& tok);
    bool handle_scan_cell16s(Parser& parser, const Token& tok);
    bool handle_alloc_array16(Parser& parser, const Token& tok);
    bool handle_free_array16(Parser& parser, const Token& tok);
    bool handle_put_array8(Parser& parser, const Token& tok);
    bool handle_put_array16(Parser& parser, const Token& tok);
    bool handle_get_array8(Parser& parser, const Token& tok);
    bool handle_get_array16(Parser& parser, const Token& tok);
    bool handle_pg_arrayX(Parser& parser, const Token& tok,
                          int width, bool put);

    bool parse_expr_args(Parser& parser,
                         const Token& tok,
                         const std::vector<std::string>& param_names,
                         std::vector<int>& values);
    bool parse_ident_arg(Parser& parser, const Token& tok,
                         std::string& ident_out);
    bool parse_string_arg(Parser& parser, const Token& tok,
                          std::string& text_out);
    Array* validate_array_name_arg(Parser& parser, const Token& tok,
                                   const std::string& macro_name);
    bool parse_array_get_put_args(Parser& parser, const Token& tok,
                                  Array*& array, int& idx_cell, int& cell);
    std::string clear_memory_area(int addr, int size16);
    std::vector<Token> substitute_body(const Macro& macro,
                                       const std::vector<std::vector<Token>>& args);
};

bool is_reserved_keyword(const std::string& name);
std::string make_temp_name();
void reset_temp_names();
