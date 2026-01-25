//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "errors.h"
#include "lexer.h"
#include "utils.h"

// CommentStripper: remove // and /* */ outside of strings (strings don't span lines)
bool CommentStripper::getline(Line& out) {
    std::string raw;
    while (true) {
        if (!g_file_stack.getline(raw)) {
            return false; // EOF
        }
        int line_num = g_file_stack.line_num() - 1;  // Line just read

        std::string clean;
        clean.reserve(raw.size());
        bool in_string = false;

        for (std::size_t i = 0; i < raw.size(); ++i) {
            char c = raw[i];

            if (in_block_comment_) {
                if (c == '*' && i + 1 < raw.size() && raw[i + 1] == '/') {
                    in_block_comment_ = false;
                    ++i; // consume '/'
                }
                continue; // skip everything inside block comment
            }

            if (!in_string) {
                if (c == '"' ) {
                    in_string = true;
                    clean.push_back(c);
                    continue;
                }
                if (c == '/' && i + 1 < raw.size() && raw[i + 1] == '/') {
                    break; // line comment: drop rest of line
                }
                if (c == '/' && i + 1 < raw.size() && raw[i + 1] == '*') {
                    in_block_comment_ = true;
                    ++i; // consume '*'
                    continue;
                }
                clean.push_back(c);
            }
            else {
                clean.push_back(c);
                if (c == '"') {
                    in_string = false;
                }
            }
        }

        out.text = std::move(clean);
        out.line_num = line_num;
        return true;
    }
}

bool CommentStripper::is_eof() const {
    return g_file_stack.is_eof();
}

Lexer::Lexer(CommentStripper& stripper)
    : stripper_(stripper) {
}

void Lexer::scan_append(const Line& line) {
    // DON'T clear tokens_ or reset pos_
    // Scan and append new tokens to the existing buffer

    in_directive_ = false;
    expr_depth_ = 0;
    size_t start_token_count = tokens_.size();

    const char* p = line.text.c_str();
    while (*p) {
        // Skip whitespace
        if (is_space(*p)) {
            ++p;
            continue;
        }

        // token start position
        const char* start = p;
        std::string filename = g_file_stack.filename();
        int line_num = line.line_num;
        int column = static_cast<int>(start - line.text.c_str() + 1);
        SourceLocation loc(filename, line_num, column);

        if (tokens_.size() == start_token_count &&
                *p == '#' && is_alpha(*(p + 1))) {
            // directive
            in_directive_ = true;
            p++;
            while (is_alpha(*p)) {
                ++p;
            }
            std::string directive(start, p - start);
            tokens_.emplace_back(TokenType::Directive, directive, loc);
            continue;
        }

        if (is_alpha(*p) || *p == '_') {
            // Identifier
            while (is_alnum(*p) || *p == '_') {
                ++p;
            }
            std::string ident(start, p - start);
            tokens_.emplace_back(TokenType::Identifier, ident, loc);
            continue;
        }

        if (is_digit(*p)) {
            // Integer
            int value = 0;
            while (is_digit(*p)) {
                value = value * 10 + (*p - '0');
                ++p;
            }
            Token t = Token::make_int(value, loc);
            tokens_.push_back(t);
            continue;
        }

        if (*p == '"') {
            // String
            ++p; // skip opening "
            std::string str;
            while (*p && *p != '"') {
                str.push_back(*p);
                ++p;
            }
            if (*p != '"') {
                // Unterminated string
                g_error_reporter.report_error(loc,
                                              "unterminated string literal"
                                             );
                tokens_.emplace_back(TokenType::Error, "", loc);
                break;
            }
            ++p; // skip closing "
            tokens_.emplace_back(TokenType::String, str, loc);
            continue;
        }

        if (p[0] == '\'' && p[2] == '\'') {
            // Char literal
            int value = static_cast<int>(static_cast<unsigned char>(p[1]));
            Token t = Token::make_int(value, loc);
            tokens_.push_back(t);
            p += 3;
            continue;
        }

        if (*p == '(') {
            // LParen
            expr_depth_++;
            std::string op(1, *p);
            p++;
            tokens_.emplace_back(TokenType::LParen, op, loc);
            continue;
        }

        if (*p == ')') {
            // RParen
            if (expr_depth_ > 0) {
                expr_depth_--;
            }
            std::string op(1, *p);
            p++;
            tokens_.emplace_back(TokenType::RParen, op, loc);
            continue;
        }

        if (expr_depth_ == 0 &&
                (*p == '+' || *p == '-' || *p == '<' || *p == '>' ||
                 *p == '[' || *p == ']' || *p == '.' || *p == ',')) {
            // BFInstr
            std::string op(1, *p);
            p++;
            tokens_.emplace_back(TokenType::BFInstr, op, loc);
            continue;
        }

        if ((in_directive_ || expr_depth_ > 0) &&
                ((p[0] == '=' && p[1] == '=') ||  // ==
                 (p[0] == '!' && p[1] == '=') ||  // !=
                 (p[0] == '<' && p[1] == '=') ||  // <=
                 (p[0] == '>' && p[1] == '=') ||  // >=
                 (p[0] == '&' && p[1] == '&') ||  // &&
                 (p[0] == '|' && p[1] == '|') ||  // ||
                 (p[0] == '<' && p[1] == '<') ||  // <<
                 (p[0] == '>' && p[1] == '>'))    // >>
           ) {
            // 2 character operator
            std::string op(p, p + 2);
            p += 2;
            tokens_.emplace_back(TokenType::Operator, op, loc);
            continue;
        }

        if ((in_directive_ || expr_depth_ > 0) &&
                (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%' ||
                 *p == '&' || *p == '|' || *p == '^' || *p == '~' ||
                 *p == '!' || *p == '<' || *p == '>')) {
            // single character operator
            std::string op(1, *p);
            p++;
            tokens_.emplace_back(TokenType::Operator, op, loc);
            continue;
        }

        // Invalid charcater
        g_error_reporter.report_error(loc,
                                      "invalid character '" + std::string(1, *p) + "'"
                                     );
        tokens_.emplace_back(TokenType::Error, "", loc);
        break;
    }

    // At end of line, append EndOfLine token
    std::string filename = g_file_stack.filename();
    int line_num = line.line_num;
    int column = static_cast<int>(line.text.size() + 1);
    SourceLocation loc(filename, line_num, column);
    tokens_.emplace_back(TokenType::EndOfLine, "", loc);
}

bool Lexer::getline(Line& line) {
    return stripper_.getline(line);
}

Token Lexer::get() {
    while (pos_ >= tokens_.size()) {
        // Compact buffer if we've consumed many tokens
        if (pos_ > 100) {
            tokens_.erase(tokens_.begin(), tokens_.begin() + pos_);
            pos_ = 0;
        }

        Line line;
        if (!getline(line)) {
            return Token(TokenType::EndOfInput, "",
                         SourceLocation(g_file_stack.filename(),
                                        g_file_stack.line_num(), 0));
        }
        scan_append(line);
    }
    return tokens_[pos_++];
}

Token Lexer::peek(size_t offset) {
    while (pos_ + offset >= tokens_.size()) {
        Line line;
        if (!getline(line)) {
            return Token(TokenType::EndOfInput, "",
                         SourceLocation(g_file_stack.filename(),
                                        g_file_stack.line_num(), 0));
        }
        scan_append(line);
    }
    return tokens_[pos_ + offset];
}

bool Lexer::at_end() const {
    if (pos_ >= tokens_.size()) {
        return true;
    }
    else {
        return false;
    }
}

bool is_identifier(const std::string& ident) {
    if (ident.empty()) {
        return false;
    }
    if (!is_alpha(ident[0]) && ident[0] != '_') {
        return false;
    }
    for (char c : ident) {
        if (!is_alnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

bool is_integer(const std::string& str) {
    if (str.empty()) {
        return false;
    }
    for (char c : str) {
        if (!is_digit(c)) {
            return false;
        }
    }
    return true;
}
