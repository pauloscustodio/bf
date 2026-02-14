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

struct StackFrame {
    SourceLocation loc;
    int start_stack_ptr = 0;
    int num_args16 = 0;
    int num_locals16 = 0;
    int num_temps16 = 0;

    int size() const;
};

class BFOutput {
public:
    BFOutput() = default;

    void put(const Token& tok);
    std::string to_string() const;
    void check_structures() const;
    int tape_ptr() const;

    // allocate cells on the heap, heap grows upwards
    int alloc_cells(int count);
    void free_cells(int addr);

    // allocate globals and temps on the heap
    // they are not freed until the end of the program
    void alloc_global(const Token& tok, int count16);
    void alloc_temp(const Token& tok, int count16);
    int global_address(const Token& tok, int n);
    int temp_address(const Token& tok, int n);

    // allocate cells on the stack, stack grows downwards
    int alloc_stack(int count);
    void free_stack(int count);
    int stack_ptr() const;

    // frames for function context
    void enter_frame(const Token& tok, int args16, int locals16);
    void leave_frame(const Token& tok);
    void frame_alloc_temp(const Token& tok, int temp16);
    int frame_arg_address(const Token& tok, int n);
    int frame_local_address(const Token& tok, int n);
    int frame_temp_address(const Token& tok, int n);

    // optimize tape movements by combining consecutive < and >
    void optimize_tape_movements();

    void reset();
    void set_stack_base(int base);
    int heap_size() const;
    int max_stack_depth() const;

private:
    static inline const int kInitialStackBase = 1000;

    int tape_ptr_ = 0;
    int heap_size_ = 0;
    int stack_base_ = kInitialStackBase;
    int stack_ptr_ = kInitialStackBase;
    int min_stack_ptr_ = kInitialStackBase;
    int global_ptr_ = -1;
    int global_count16_ = 0;
    int temp_ptr_ = -1;
    int temp_count16_ = 0;
    std::vector<StackFrame> frame_stack_;
    std::vector<SourceLocation> loop_stack_;
    std::vector<Token> output_;

    // heap management
    // free_list_: vector of {start, length}, kept merged and non-overlapping
    std::vector<std::pair<int, int>> free_list_;
    // alloc_map_: start -> length
    std::unordered_map<int, int> alloc_map_;

    void add_free_block(int start, int len);

};
