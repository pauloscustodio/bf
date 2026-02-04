//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "output.h"
#include "errors.h"
#include <algorithm>

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

    for (const Token& t : output_) {
        // synchronize line numbers with SourceLocation
        while (line_num < t.loc.line_num) {
            result += '\n';
            line_num++;
            at_line_start = true;
        }

        if (t.text == "[") {
            // Output '[' with current indentation on its own line
            if (!at_line_start) {
                result += '\n';
                line_num++;
            }
            result += std::string(indent_level * 2, ' ');
            result += "[\n";
            line_num++;
            indent_level++;
            at_line_start = true;
        }
        else if (t.text == "]") {
            // Decrease indent, then output ']' on its own line
            if (!at_line_start) {
                result += '\n';
                line_num++;
            }
            indent_level--;
            result += std::string(indent_level * 2, ' ');
            result += "]\n";
            line_num++;
            at_line_start = true;
        }
        else {
            // Regular BF instruction with indentation only at line start
            if (at_line_start) {
                result += std::string(indent_level * 2, ' ');
            }
            result += t.text;
            at_line_start = false;
        }
    }
    if (!at_line_start) {
        result += '\n';
    }
    return result;
}

void BFOutput::check_loops() const {
    for (auto& it : loop_stack_) {
        g_error_reporter.report_error(
            it,
            "unmatched '[' instruction"
        );
    }
}

int BFOutput::tape_ptr() const {
    return tape_ptr_;
}

int BFOutput::alloc_cells(int count) {
    if (count <= 0) {
        return reserved_ptr_; // no-op, but defined behavior
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
    int alloc_start = reserved_ptr_;
    reserved_ptr_ += count;
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

    // Optionally shrink reserved_ptr_ if the topmost range reaches the end
    if (!free_list_.empty()) {
        auto& last = free_list_.back();
        int last_end = last.first + last.second;
        if (last_end == reserved_ptr_) {
            // pull reserved_ptr_ down
            reserved_ptr_ = last.first;
            free_list_.pop_back();
        }
    }
}

void BFOutput::optimize_tape_movements() {
    std::vector<Token> optimized;
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
