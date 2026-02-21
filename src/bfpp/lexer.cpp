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
    int base_line_num = 0;
    bool have_any = false;

    // Gather continuation lines ending with backslash
    while (true) {
        std::string segment;
        if (!g_file_stack.getline(segment)) {
            if (!have_any) {
                return false;    // EOF, nothing read
            }
            break; // EOF after a continued line: return what we have
        }

        int seg_line = g_file_stack.line_num() - 1; // line just read
        if (!have_any) {
            base_line_num = seg_line;
            have_any = true;
        }

        raw += segment;

        // Continuation if last char is backslash
        if (!segment.empty() && segment.back() == '\\') {
            raw.pop_back(); // drop the backslash
            raw.push_back(' '); // replace with space
            continue;       // keep reading next physical line
        }
        break; // no continuation
    }

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
    out.line_num = base_line_num;
    return true;
}

bool CommentStripper::is_eof() const {
    return g_file_stack.is_eof();
}

Token::Token(TokenType t, const std::string& txt, const SourceLocation& loc)
    : type(t), text(txt), loc(loc) {
}

bool Token::is_comma() const {
    return type == TokenType::BFInstr && text == ",";
}

Token Token::make_bf(char c, const SourceLocation& loc) {
    Token t;
    t.type = TokenType::BFInstr;
    t.text.assign(1, c);
    t.loc = loc;
    return t;
}

Token Token::make_int(int value, const SourceLocation& loc) {
    Token t;
    t.type = TokenType::Integer;
    t.int_value = value;
    t.text = std::to_string(value);
    t.loc = loc;
    return t;
}

void TokenScanner::scan_line(const std::string& text,
                             const std::string& filename,
                             int line_num,
                             std::vector<Token>& tokens,
                             bool& in_directive,
                             int& expr_depth) const {
    size_t start_token_count = tokens.size();

    const char* p = text.c_str();
    while (*p) {
        if (*p != '\n' && is_space(*p)) {
            ++p;
            continue;
        }

        if (*p == '\n') {
            ++p;
            int column = static_cast<int>(p - text.c_str() + 1);
            SourceLocation loc(filename, line_num, column);
            tokens.emplace_back(TokenType::EndOfLine, "", loc);
            in_directive = false;
            expr_depth = 0;
            start_token_count = tokens.size();
            continue;
        }

        const char* start = p;
        int column = static_cast<int>(start - text.c_str() + 1);
        SourceLocation loc(filename, line_num, column);

        if (tokens.size() == start_token_count &&
                *p == '#' && is_alpha(*(p + 1))) {
            in_directive = true;
            p++;
            while (is_alpha(*p)) {
                ++p;
            }
            std::string directive(start, p - start);
            tokens.emplace_back(TokenType::Directive, directive, loc);
            continue;
        }

        if (is_alpha(*p) || *p == '_') {
            while (is_alnum(*p) || *p == '_') {
                ++p;
            }
            std::string ident(start, p - start);
            tokens.emplace_back(TokenType::Identifier, ident, loc);
            continue;
        }

        if (is_digit(*p)) {
            int value = 0;
            while (is_digit(*p)) {
                value = value * 10 + (*p - '0');
                ++p;
            }
            Token t = Token::make_int(value, loc);
            tokens.push_back(t);
            continue;
        }

        if (*p == '"') {
            ++p;
            std::string str;
            while (*p && *p != '"') {
                if (!in_directive && *p == '\\' && *(p + 1)) {
                    // Process escape sequences (except in directives for path separators)
                    ++p; // consume backslash
                    switch (*p) {
                    case 'n':
                        str.push_back('\n');
                        break;
                    case 't':
                        str.push_back('\t');
                        break;
                    case 'r':
                        str.push_back('\r');
                        break;
                    case '\\':
                        str.push_back('\\');
                        break;
                    case '"':
                        str.push_back('"');
                        break;
                    case '\'':
                        str.push_back('\'');
                        break;
                    case '0':
                        str.push_back('\0');
                        break;
                    case 'a':
                        str.push_back('\a');
                        break;
                    case 'b':
                        str.push_back('\b');
                        break;
                    case 'f':
                        str.push_back('\f');
                        break;
                    case 'v':
                        str.push_back('\v');
                        break;
                    default:
                        // Unknown escape: keep backslash and character
                        g_error_reporter.report_error(loc,
                                                      "unknown escape sequence '\\" + std::string(1, *p) + "'");
                        str.push_back('\\');
                        str.push_back(*p);
                        break;
                    }
                    ++p;
                }
                else {
                    str.push_back(*p);
                    ++p;
                }
            }
            if (*p != '"') {
                g_error_reporter.report_error(loc,
                                              "unterminated string literal");
                break;
            }
            ++p;
            tokens.emplace_back(TokenType::String, str, loc);
            continue;
        }

        if (p[0] == '\'' && p[2] == '\'') {
            int value = static_cast<int>(static_cast<unsigned char>(p[1]));
            Token t = Token::make_int(value, loc);
            tokens.push_back(t);
            p += 3;
            continue;
        }

        if (*p == '(') {
            expr_depth++;
            std::string op(1, *p);
            p++;
            tokens.emplace_back(TokenType::LParen, op, loc);
            continue;
        }

        if (*p == ')') {
            if (expr_depth > 0) {
                expr_depth--;
            }
            std::string op(1, *p);
            p++;
            tokens.emplace_back(TokenType::RParen, op, loc);
            continue;
        }

        if (*p == '{') {
            std::string op(1, *p);
            p++;
            tokens.emplace_back(TokenType::LBrace, op, loc);
            continue;
        }

        if (*p == '}') {
            std::string op(1, *p);
            p++;
            tokens.emplace_back(TokenType::RBrace, op, loc);
            continue;
        }

        if (*p == ',') {
            std::string op(1, *p);
            p++;
            tokens.emplace_back(TokenType::BFInstr, op, loc);
            continue;
        }

        if (expr_depth == 0 &&
                (*p == '+' || *p == '-' || *p == '<' || *p == '>' ||
                 *p == '[' || *p == ']' || *p == '.' || *p == ',')) {
            std::string op(1, *p);
            p++;
            tokens.emplace_back(TokenType::BFInstr, op, loc);
            continue;
        }

        if ((in_directive || expr_depth > 0) &&
                ((p[0] == '=' && p[1] == '=') ||
                 (p[0] == '!' && p[1] == '=') ||
                 (p[0] == '<' && p[1] == '=') ||
                 (p[0] == '>' && p[1] == '=') ||
                 (p[0] == '&' && p[1] == '&') ||
                 (p[0] == '|' && p[1] == '|') ||
                 (p[0] == '<' && p[1] == '<') ||
                 (p[0] == '>' && p[1] == '>'))) {
            std::string op(p, p + 2);
            p += 2;
            tokens.emplace_back(TokenType::Operator, op, loc);
            continue;
        }

        if ((in_directive || expr_depth > 0) &&
                (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%' ||
                 *p == '&' || *p == '|' || *p == '^' || *p == '~' ||
                 *p == '!' || *p == '<' || *p == '>')) {
            std::string op(1, *p);
            p++;
            tokens.emplace_back(TokenType::Operator, op, loc);
            continue;
        }

        g_error_reporter.report_error(loc,
                                      "invalid character '" + std::string(1, *p) + "'");
        break;
    }

    int column = static_cast<int>(text.size() + 1);
    SourceLocation eol_loc(filename, line_num, column);
    tokens.emplace_back(TokenType::EndOfLine, "", eol_loc);
}

std::vector<Token> TokenScanner::scan_string(const std::string& text,
        const std::string& filename,
        int line_num) const {
    std::vector<Token> tokens;
    bool in_directive = false;
    int expr_depth = 0;
    scan_line(text, filename, line_num, tokens, in_directive, expr_depth);
    return tokens;
}

Lexer::Lexer(CommentStripper& stripper)
    : stripper_(stripper) {
}

void Lexer::scan_append(const Line& line) {
    in_directive_ = false;
    expr_depth_ = 0;

    TokenScanner scanner;
    scanner.scan_line(line.text,
                      g_file_stack.filename(),
                      line.line_num,
                      tokens_,
                      in_directive_,
                      expr_depth_);
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

