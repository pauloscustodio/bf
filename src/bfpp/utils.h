//-----------------------------------------------------------------------------
// Brainfuck Preprocessor
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#pragma once

#include <string>

bool is_space(int ch);
bool is_alpha(int ch);
bool is_digit(int ch);
bool is_alnum(int ch);
std::string lowercase(std::string s);

