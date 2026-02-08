//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include "files.h"
#include "lexer.h"
#include <string>
#include <vector>
#include <unordered_map>

class BFOutput {
public:
    static inline const int kInitialStackBase = 1000;
    static inline const int kMinHeapToStackDistance = 30;

    BFOutput() = default;

    void put(const Token& tok);
    std::string to_string() const;
    void check_loops() const;
    int tape_ptr() const;

    // allocate cells on the tape
    int alloc_cells(int count);
    void free_cells(int addr);

    // allocate cells on the stack, stack grows downwards
    int alloc_stack(int count);
    void free_stack(int count);

    // optimize tape movements by combining consecutive < and >
    void optimize_tape_movements();

    void reset();
    void set_stack_base(int base);
    int heap_size() const;
    int max_stack_depth() const;

private:
    int tape_ptr_ = 0;
    int heap_size_ = 0;
    int stack_base_ = kInitialStackBase;
    int stack_top_ = kInitialStackBase;
    int min_stack_top_ = kInitialStackBase;
    std::vector<SourceLocation> loop_stack_;
    std::vector<Token> output_;

    // heap management
    // free_list_: vector of {start, length}, kept merged and non-overlapping
    std::vector<std::pair<int, int>> free_list_;
    // alloc_map_: start -> length
    std::unordered_map<int, int> alloc_map_;

    void add_free_block(int start, int len);

};
