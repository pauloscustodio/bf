//-----------------------------------------------------------------------------
// Brainfuck interpreter
// Copyright (c) Paulo Custodio 2026
// License: The Artistic License 2.0, http ://www.perlfoundation.org/artistic_license_2_0
//-----------------------------------------------------------------------------

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stack>
#include <vector>

// global data
std::vector<uint8_t> tape;
std::vector<char> code;
std::stack<size_t> loop_stack;
size_t ptr = 0;         // pointer to the tape
size_t pc = 0;          // program counter

void error(const std::string& msg) {
    std::cerr << "Error: " << msg << std::endl;
    std::exit(1);
}

void usage_error() {
    error("Usage: bf [-t] [-D] [file]");
}

void read_code(std::istream& in) {
    code.clear();
    char ch;
    while (in.get(ch)) {
        if (ch == '>' || ch == '<' || ch == '+' || ch == '-' ||
                ch == '.' || ch == ',' || ch == '[' || ch == ']') {
            code.push_back(ch);
        }
    }
}

static void dump_state() {
    std::cout << "Tape:";
    for (size_t i = 0; i < tape.size(); ++i) {
        std::cout << std::setw(3) << static_cast<int>(tape[i]) << ' ';
    }
    std::cout << "\n     ";                     // align under first cell after "Tape:"
    if (ptr > 0) {
        std::cout << std::string(ptr * 4, ' '); // spaces to cell before ptr
    }
    std::cout << "^^^"                          // arrow points to the cell slot
              << " (ptr=" << ptr << ")\n\n";
}

void run_machine(bool trace) {
    pc = ptr = 0;
    tape.clear();
    loop_stack = std::stack<size_t>();
    tape.push_back(0); // initialize tape with one cell

    while (pc < code.size()) {
        if (trace) {
            std::cout << "PC=" << pc << " instr='" << code[pc] << "'\n";
        }

        switch (code[pc]) {
        case '>':
            while (ptr + 1 >= tape.size()) {
                tape.push_back(0);
            }
            ptr++;
            break;
        case '<':
            if (ptr == 0) {
                error("Tape pointer underflow");
            }
            ptr--;
            break;
        case '+':
            tape[ptr]++;
            break;
        case '-':
            tape[ptr]--;
            break;
        case '.':
            std::cout.put(static_cast<char>(tape[ptr]));
            break;
        case ',':
            tape[ptr] = static_cast<uint8_t>(std::cin.get());
            break;
        case '[':
            if (tape[ptr] == 0) {
                size_t open_brackets = 1;
                while (open_brackets > 0) {
                    if (++pc >= code.size()) {
                        error("Unmatched '['");
                    }
                    if (code[pc] == '[') {
                        open_brackets++;
                    }
                    else if (code[pc] == ']') {
                        open_brackets--;
                    }
                }
            }
            else {
                loop_stack.push(pc);
            }
            break;
        case ']':
            if (loop_stack.empty()) {
                error("Unmatched ']'");
            }
            if (tape[ptr] != 0) {
                pc = loop_stack.top();
            }
            else {
                loop_stack.pop();
            }
            break;
        }

        if (trace) {
            dump_state();
        }

        pc++;
    }
}

int main(int argc, char* argv[]) {
    bool trace = false;
    bool dump_after = false;
    const char* filename = nullptr;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "-t") == 0) {
            trace = true;
        }
        else if (std::strcmp(arg, "-D") == 0) {
            dump_after = true;
        }
        else if (arg[0] == '-') {
            usage_error();
        }
        else if (filename == nullptr) {
            filename = arg;
        }
        else {
            usage_error();
        }
    }

    if (filename == nullptr) {
        read_code(std::cin);
    }
    else {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            error("Cannot open file: " + std::string(filename));
        }
        read_code(file);
    }

    run_machine(trace);

    if (dump_after) {
        dump_state();
    }

    return 0;
}
