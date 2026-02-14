//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "output.h"
#include "errors.h"
#include <algorithm>

int StackFrame::size() const {
    return 2 * (num_args16 + num_locals16 + num_temps16);
}

void BFOutput::put(const Token& tok) {
    if (tok.type != TokenType::BFInstr) {
        g_error_reporter.report_error(
            tok.loc,
            "non-BF instruction token in output: '" + tok.text + "'"
        );
        return;
    }

    if (tok.text == ">") {
        tape_ptr_++;
    }
    else if (tok.text == "<") {
        if (tape_ptr_ == 0) {
            g_error_reporter.report_error(
                tok.loc,
                "tape pointer moved to negative position"
            );
            return;
        }
        tape_ptr_--;
    }
    else if (tok.text == "[") {
        loop_stack_.push_back(tok.loc);
    }
    else if (tok.text == "]") {
        if (loop_stack_.empty()) {
            g_error_reporter.report_error(
                tok.loc,
                "unmatched ']' instruction"
            );
            return;
        }
        loop_stack_.pop_back();
    }

    output_.push_back(tok);
}

std::string BFOutput::to_string() const {
    std::string result;
    int line_num = 1;
    int indent_level = 0;
    bool at_line_start = true;
    int line_len = 0; // current line length including indentation

    auto start_new_line = [&](int new_indent_level) {
        result += '\n';
        line_num++;
        at_line_start = true;
        line_len = 0;
        indent_level = new_indent_level;
    };

    auto ensure_wrap_before = [&](int needed_len, int indent_spaces) {
        if (line_len + needed_len > 80) {
            result += '\n';
            line_num++;
            at_line_start = true;
            line_len = 0;
            result += std::string(indent_spaces, ' ');
            line_len += indent_spaces;
        }
    };

    for (const Token& t : output_) {
        // synchronize line numbers with SourceLocation
        while (line_num < t.loc.line_num) {
            result += '\n';
            line_num++;
            at_line_start = true;
            line_len = 0;
        }

        if (t.text == "[") {
            // Output '[' with current indentation on its own line
            if (!at_line_start) {
                start_new_line(indent_level);
            }
            int indent_spaces = indent_level * 2;
            result += std::string(indent_spaces, ' ');
            result += "[\n";
            line_num++;
            indent_level++;
            at_line_start = true;
            line_len = 0;
        }
        else if (t.text == "]") {
            // Decrease indent, then output ']' on its own line
            if (!at_line_start) {
                start_new_line(indent_level);
            }
            indent_level--;
            int indent_spaces = indent_level * 2;
            result += std::string(indent_spaces, ' ');
            result += "]\n";
            line_num++;
            at_line_start = true;
            line_len = 0;
        }
        else {
            int indent_spaces = at_line_start ? indent_level * 2 : 0;
            int needed_len = indent_spaces + static_cast<int>(t.text.size());

            if (at_line_start) {
                // apply wrapping considering indentation + token
                if (needed_len > 80) {
                    // token with indent alone exceeds line; still place it on new line
                    result += std::string(indent_spaces, ' ');
                    result += t.text;
                    at_line_start = false;
                    line_len = indent_spaces + static_cast<int>(t.text.size());
                }
                else {
                    ensure_wrap_before(needed_len, indent_spaces);
                    if (line_len == 0 && indent_spaces > 0) {
                        result += std::string(indent_spaces, ' ');
                        line_len += indent_spaces;
                    }
                    result += t.text;
                    line_len += static_cast<int>(t.text.size());
                    at_line_start = false;
                }
            }
            else {
                // not at line start; wrap if needed before adding token
                ensure_wrap_before(static_cast<int>(t.text.size()), indent_level * 2);
                result += t.text;
                line_len += static_cast<int>(t.text.size());
            }
        }
    }
    if (!at_line_start) {
        result += '\n';
    }
    return result;
}

void BFOutput::check_structures() const {
    for (auto& it : loop_stack_) {
        g_error_reporter.report_error(
            it,
            "unmatched '[' instruction"
        );
    }
    for (auto& it : frame_stack_) {
        g_error_reporter.report_error(
            it.loc,
            "unmatched enter_frame16 instruction"
        );
    }
}

int BFOutput::tape_ptr() const {
    return tape_ptr_;
}

int BFOutput::alloc_cells(int count) {
    if (count <= 0) {
        return heap_size_; // no-op, but defined behavior
    }

    // First-fit search in free list
    for (std::size_t i = 0; i < free_list_.size(); ++i) {
        int start = free_list_[i].first;
        int len   = free_list_[i].second;
        if (len >= count) {
            // allocate from this block
            int alloc_start = start;
            int remaining = len - count;
            if (remaining == 0) {
                free_list_.erase(free_list_.begin() + static_cast<long>(i));
            }
            else {
                free_list_[i].first = start + count;
                free_list_[i].second = remaining;
            }
            alloc_map_[alloc_start] = count;
            return alloc_start;
        }
    }

    // No free block: extend high watermark
    int alloc_start = heap_size_;
    heap_size_ += count;
    alloc_map_[alloc_start] = count;
    return alloc_start;
}

void BFOutput::add_free_block(int start, int len) {
    if (len <= 0) {
        return;
    }
    // insert and merge
    free_list_.push_back({start, len});
    std::sort(free_list_.begin(), free_list_.end(),
    [](auto & a, auto & b) {
        return a.first < b.first;
    });

    std::vector<std::pair<int, int>> merged;
    for (auto& blk : free_list_) {
        if (merged.empty()) {
            merged.push_back(blk);
            continue;
        }
        auto& back = merged.back();
        int back_start = back.first;
        int back_end = back.first + back.second;
        int blk_start = blk.first;
        int blk_end = blk.first + blk.second;

        if (blk_start <= back_end) { // overlap or adjacent
            int new_end = std::max(back_end, blk_end);
            back.second = new_end - back_start;
        }
        else {
            merged.push_back(blk);
        }
    }
    free_list_.swap(merged);

    // Note: we cannot shrink heap size
    // because bfpp needs to know the maximum heap size used
    // to know where to place the stack
#if 0
    // Optionally shrink heap_size_ if the topmost range reaches the end
    if (!free_list_.empty()) {
        auto& last = free_list_.back();
        int last_end = last.first + last.second;
        if (last_end == heap_size_) {
            // pull heap_size_ down
            heap_size_ = last.first;
            free_list_.pop_back();
        }
    }
#endif
}

void BFOutput::optimize_tape_movements() {
    std::vector<Token> optimized;
    optimized.reserve(output_.size());
    int net_move = 0;
    for (const Token& t : output_) {
        if (t.type == TokenType::BFInstr) {
            if (t.text == ">") {
                net_move++;
            }
            else if (t.text == "<") {
                net_move--;
            }
            else {
                // flush any pending moves
                if (net_move > 0) {
                    for (int i = 0; i < net_move; ++i) {
                        optimized.push_back(Token::make_bf('>', t.loc));
                    }
                }
                else if (net_move < 0) {
                    for (int i = 0; i < -net_move; ++i) {
                        optimized.push_back(Token::make_bf('<', t.loc));
                    }
                }
                net_move = 0;
                optimized.push_back(t);
            }
        }
        else {
            optimized.push_back(t);
        }
    }
    // flush any remaining moves at the end
    if (net_move > 0) {
        for (int i = 0; i < net_move; ++i) {
            optimized.push_back(Token::make_bf('>', SourceLocation()));
        }
    }
    else if (net_move < 0) {
        for (int i = 0; i < -net_move; ++i) {
            optimized.push_back(Token::make_bf('<', SourceLocation()));
        }
    }
    output_.swap(optimized);
}

void BFOutput::free_cells(int addr) {
    auto it = alloc_map_.find(addr);
    if (it == alloc_map_.end()) {
        g_error_reporter.report_error(
            SourceLocation(),
            "attempt to free unknown allocation at address " + std::to_string(addr)
        );
        return;
    }

    int len = it->second;
    alloc_map_.erase(it);
    add_free_block(addr, len);
}

int BFOutput::alloc_global(const Token& tok, int count16) {
    if (count16 <= 0) {
        return -1; // no-op, but defined behavior
    }
    if (global_ptr_ == -1) {
        global_ptr_ = alloc_cells(count16 * 2);
        global_count16_ = count16;
        return global_ptr_;
    }
    else {
        g_error_reporter.report_error(
            tok.loc,
            "alloc_global16 already called"
        );
        return -1;
    }
}

void BFOutput::free_global() {
    if (global_ptr_ == -1) {
        return; // no-op, but defined behavior
    }
    free_cells(global_ptr_);
    global_ptr_ = -1;
    global_count16_ = 0;
}

int BFOutput::alloc_temp(const Token& tok, int count16) {
    if (count16 <= 0) {
        return -1; // no-op, but defined behavior
    }
    if (temp_ptr_ == -1) {
        temp_ptr_ = alloc_cells(count16 * 2);
        temp_count16_ = count16;
        return temp_ptr_;
    }
    else {
        g_error_reporter.report_error(
            tok.loc,
            "alloc_temp16 already called"
        );
        return -1;
    }
}

void BFOutput::free_temp() {
    if (temp_ptr_ == -1) {
        return; // no-op, but defined behavior
    }
    free_cells(temp_ptr_);
    temp_ptr_ = -1;
    temp_count16_ = 0;
}

int BFOutput::global_address(const Token& tok, int n) {
    if (global_ptr_ == -1) {
        g_error_reporter.report_error(
            tok.loc,
            "global() called before alloc_global16"
        );
        return -1;
    }
    if (n < 0 || n >= global_count16_) {
        g_error_reporter.report_error(
            tok.loc,
            "global(" + std::to_string(n) + ") overflow"
        );
        return -1;
    }
    return global_ptr_ + 2 * n;
}

int BFOutput::temp_address(const Token& tok, int n) {
    if (temp_ptr_ == -1) {
        g_error_reporter.report_error(
            tok.loc,
            "temp() called before alloc_temp16"
        );
        return -1;
    }
    if (n < 0 || n >= temp_count16_) {
        g_error_reporter.report_error(
            tok.loc,
            "temp(" + std::to_string(n) + ") overflow"
        );
        return -1;
    }
    return temp_ptr_ + 2 * n;
}

int BFOutput::alloc_stack(int count) {
    if (count <= 0) {
        return stack_ptr_; // no-op, but defined behavior
    }
    if (stack_ptr_ - count < heap_size_) {
        g_error_reporter.report_error(
            SourceLocation(),
            "stack overflow: not enough space between heap and stack for allocation of " + std::to_string(count) + " cells"
        );
        return stack_ptr_; // return current top as fallback
    }

    stack_ptr_ -= count;
    min_stack_ptr_ = std::min(min_stack_ptr_, stack_ptr_);
    return stack_ptr_;
}

void BFOutput::free_stack(int count) {
    if (count <= 0) {
        return; // no-op, but defined behavior
    }
    if (stack_ptr_ + count > stack_base_) {
        g_error_reporter.report_error(
            SourceLocation(),
            "stack underflow: attempt to free more stack cells than allocated"
        );
        return;
    }

    stack_ptr_ += count;
}

int BFOutput::stack_ptr() const {
    return stack_ptr_;
}

void BFOutput::enter_frame(const Token& tok, int args16, int locals16) {
    if (args16 < 0 || locals16 < 0) {
        return; // no-op, but defined behavior
    }

    // Ensure the arguments are already present on the stack.
    // Each arg is 16-bit => 2 cells.
    int args_cells = 2 * args16;
    int available = stack_base_ - stack_ptr_;
    if (available < args_cells) {
        g_error_reporter.report_error(
            tok.loc,
            "enter_frame: not enough arguments on stack (expected " +
            std::to_string(args16) + " x16-bit)"
        );
        return;
    }

    // reserve arg0 for return value
    if (args16 == 0) {
        alloc_stack(2);
        args16++;
        args_cells = 2 * args16;
    }

    // create frame
    StackFrame frame;
    frame.loc = tok.loc;
    frame.start_stack_ptr = stack_ptr_;
    frame.num_args16 = args16;
    frame.num_locals16 = locals16;
    frame.num_temps16 = 0;
    frame_stack_.push_back(frame);

    // space to reserve on stack: whole frame less args16, these are
    // already on the stack when enter_frame() is called
    int reserve_size = frame.size() - args_cells;
    alloc_stack(reserve_size);
}

void BFOutput::leave_frame(const Token& tok) {
    if (frame_stack_.empty()) {
        g_error_reporter.report_error(
            tok.loc,
            "unmatched leave_frame16 instruction"
        );
        return;
    }

    // drop frame, keep arg0 on the stack - return value
    StackFrame frame = frame_stack_.back();
    frame_stack_.pop_back();

    int free_size = frame.size() - 2 * (frame.num_args16 - 1);
    free_stack(free_size);
}

void BFOutput::frame_alloc_temp(const Token& tok, int temp16) {
    if (frame_stack_.empty()) {
        g_error_reporter.report_error(
            tok.loc,
            "alloc_temp16 instruction outside alloc_frame16"
        );
        return;
    }

    StackFrame& frame = frame_stack_.back();
    frame.num_temps16 += temp16;
    alloc_stack(2 * temp16);
}

int BFOutput::frame_arg_address(const Token& tok, int n) {
    if (frame_stack_.empty()) {
        g_error_reporter.report_error(
            tok.loc,
            "arg() instruction outside alloc_frame16"
        );
        return -1;
    }

    StackFrame& frame = frame_stack_.back();
    if (n < 0 || n >= frame.num_args16) {
        g_error_reporter.report_error(
            tok.loc,
            "arg(" + std::to_string(n) + ") overflow"
        );
        return -1;
    }

    int addr = frame.start_stack_ptr + 2 * (frame.num_args16 - n - 1);
    return addr;
}

int BFOutput::frame_local_address(const Token& tok, int n) {
    if (frame_stack_.empty()) {
        g_error_reporter.report_error(
            tok.loc,
            "local() instruction outside alloc_frame16"
        );
        return -1;
    }

    StackFrame& frame = frame_stack_.back();
    if (n < 0 || n >= frame.num_locals16) {
        g_error_reporter.report_error(
            tok.loc,
            "local(" + std::to_string(n) + ") overflow"
        );
        return -1;
    }

    int addr = frame.start_stack_ptr - 2 * (n + 1);
    return addr;
}

int BFOutput::frame_temp_address(const Token& tok, int n) {
    if (frame_stack_.empty()) {
        g_error_reporter.report_error(
            tok.loc,
            "frame_temp() instruction outside alloc_frame16"
        );
        return -1;
    }

    StackFrame& frame = frame_stack_.back();
    if (n < 0 || n >= frame.num_temps16) {
        g_error_reporter.report_error(
            tok.loc,
            "frame_temp(" + std::to_string(n) + ") overflow"
        );
        return -1;
    }

    int addr = frame.start_stack_ptr - 2 * (frame.num_locals16 + n + 1);
    return addr;
}

void BFOutput::reset() {
    output_.clear();
    frame_stack_.clear();
    loop_stack_.clear();
    free_list_.clear();
    alloc_map_.clear();
    tape_ptr_ = 0;
    heap_size_ = 0;
    stack_base_ = stack_ptr_ = min_stack_ptr_ = kInitialStackBase;
    global_ptr_ = -1;
    global_count16_ = 0;
    temp_ptr_ = -1;
    temp_count16_ = 0;

}

void BFOutput::set_stack_base(int base) {
    stack_base_ = stack_ptr_ = min_stack_ptr_ = base;
}

int BFOutput::heap_size() const {
    return heap_size_;
}

int BFOutput::max_stack_depth() const {
    return stack_base_ - min_stack_ptr_;
}

