//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include "utils.h"
#include <cctype>

bool is_space(int ch) {
    return std::isspace(static_cast<unsigned char>(ch));
}

bool is_alpha(int ch) {
    return std::isalpha(static_cast<unsigned char>(ch));
}

bool is_digit(int ch) {
    return std::isdigit(static_cast<unsigned char>(ch));
}

bool is_alnum(int ch) {
    return std::isalnum(static_cast<unsigned char>(ch));
}

std::string lowercase(std::string s) {
    for (char& c : s) {
        c = std::tolower(c);
    }
    return s;
}
