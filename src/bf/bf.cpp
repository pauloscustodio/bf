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
#include <sstream>
#include <stack>
#include <vector>

void error(const std::string& msg) {
    std::cerr << "Error: " << msg << std::endl;
    exit(EXIT_FAILURE);
}

void usage_error() {
    std::cerr << "usage: bf [-t] [-D] [input_file]" << std::endl;
    exit(EXIT_FAILURE);
}

enum class OpType {
    Move,
    Clear, 		// tape[ptr] = 0
    Increment, 	// tape[ptr] += value
    Multiply, 	// tape[ptr + value] += tape[ptr] * factor
    Scan, 		// move pointer until zero is found, value is direction
    StartLoop,
    EndLoop,
    Input,
    Output,
};

struct MultiplyTarget {
    int offset;
    int factor;
};

struct Op {
    OpType type = OpType::Move;
    int value = 0;
    std::vector<MultiplyTarget> targets;

    Op(OpType type_, int value_ = 0)
        : type(type_), value(value_) {}

    std::string to_string() const;
};

std::string Op::to_string() const {
    std::ostringstream oss;

    switch (type) {
    case OpType::Move:
        oss << "Move" << "(" << value << ")";
        break;
    case OpType::Clear:
        oss << "Clear" << "()";
        break;
    case OpType::Increment:
        oss << "Increment" << "(" << value << ")";
        break;
    case OpType::Multiply:
        oss << "Multiply" << "(";
        for (const auto& target : targets) {
            oss << "[" << target.offset << ":" << target.factor << "]";
        }
        oss << ")";
        break;
    case OpType::Scan:
        oss << "Scan" << "(" << value << ")";
        break;
    case OpType::StartLoop:
        oss << "StartLoop" << "(" << value << ")";
        break;
    case OpType::EndLoop:
        oss << "EndLoop" << "(" << value << ")";
        break;
    case OpType::Input:
        oss << "Input" << "(" << value << ")";
        break;
    case OpType::Output:
        oss << "Output" << "(" << value << ")";
        break;
    default:
        error("Invalid op");
    }
    return oss.str();
}

class BFVM {
public:
    void read_code(std::istream& in);
    void compile_code();
    void run();

    void set_trace(bool f = true) {
        trace = f;
    }
    void dump_state() const;

private:
    std::vector<uint8_t> tape;
    std::vector<char> code;
    std::vector<Op> ops;
    std::vector<int> jumps;

    int ptr = 0;         // pointer to the tape
    int pc = 0;          // program counter
    bool trace = false;

    void translate_ops();
    void compute_jumps();
};

void BFVM::read_code(std::istream& in) {
    code.clear();
    int nesting = 0;
    int tape_pos = 0;

    char ch;
    while (in >> ch) {
        // quick filter
        if (ch == '>' || ch == '<' || ch == '+' || ch == '-' ||
                ch == '.' || ch == ',' || ch == '[' || ch == ']') {

            if (ch == '>') {
                tape_pos++;
            }
            else if (ch == '<') {
                if (--tape_pos < 0) {
                    error("Tape pointer underflow");
                }
            }
            else if (ch == '[') {
                nesting++;
            }
            else if (ch == ']') {
                if (--nesting < 0) {
                    error("Unmatched ']'");
                }
            }
            code.push_back(ch);
        }
    }

    if (nesting != 0) {
        error("Unmatched '['");
    }
}

void BFVM::compile_code() {
    translate_ops();
    compute_jumps();
}

void BFVM::translate_ops() {
    ops.clear();
    size_t in = 0;
    while (in < code.size()) {
        if (code[in] == '<' || code[in] == '>') {
            int movement = 0;
            for (size_t i = in; i < code.size() && (code[i] == '<' || code[i] == '>'); i++) {
                if (code[i] == '<') {
                    movement--;
                }
                else {
                    movement++;
                }
                in++;
            }
            if (movement != 0) {
                ops.push_back(Op(OpType::Move, movement));
            }
            continue;
        }

        if (code[in] == '+' || code[in] == '-') {
            int increment = 0;
            for (size_t i = in; i < code.size() && (code[i] == '+' || code[i] == '-'); i++) {
                if (code[i] == '+') {
                    increment++;
                }
                else {
                    increment--;
                }
                in++;
            }
            if (increment != 0) {
                ops.push_back(Op(OpType::Increment, increment));
            }
            continue;
        }

        if (in + 2 < code.size() && code[in] == '[' && code[in + 1] == '-' && code[in + 2] == ']') {
            ops.push_back(Op(OpType::Clear));
            in += 3;
            continue;
        }

        // find for scan
        if (code[in] == '[') {
            // detects [>] or [<]
            if (in + 2 < code.size() && (code[in + 1] == '>' || code[in + 1] == '<') && code[in + 2] == ']') {
                int direction = (code[in + 1] == '>') ? 1 : -1;
                ops.push_back(Op(OpType::Scan, direction));
                in += 3;
                continue;
            }
        }

        // find for multiply with multiple targets
        if (code[in] == '[') {
            size_t scan = in + 1;
            if (scan < code.size() && code[scan] == '-') {
                scan++;
                std::vector<MultiplyTarget> targets;
                int current_offset = 0;

                // loop[ to capture multiple destinations: >B + >T +
                while (scan < code.size() &&
                        (code[scan] == '>' || code[scan] == '<' ||
                         code[scan] == '+' || code[scan] == '-')) {
                    // accumulate movements
                    int move = 0;
                    while (scan < code.size() &&
                            (code[scan] == '>' || code[scan] == '<')) {
                        move += (code[scan] == '>') ? 1 : -1;
                        scan++;
                    }
                    current_offset += move;

                    // accumulate increments for this offset
                    int factor = 0;
                    while (scan < code.size() &&
                            (code[scan] == '+' || code[scan] == '-')) {
                        factor += (code[scan] == '+') ? 1 : -1;
                        scan++;
                    }

                    if (factor != 0)
                        targets.push_back({current_offset, factor});
                }

                // verify if the loop is closed and returned to original cell
                // (current_offset == 0)
                if (scan < code.size() && code[scan] == ']' &&
                        current_offset == 0 && !targets.empty()) {
                    Op multi_op(OpType::Multiply);
                    multi_op.targets = std::move(targets);
                    ops.push_back(multi_op);
                    in = scan + 1;
                    continue;
                }
            }
        }

        if (code[in] == '[') {
            ops.push_back(Op(OpType::StartLoop));
            in++;
            continue;
        }

        if (code[in] == ']') {
            ops.push_back(Op(OpType::EndLoop));
            in++;
            continue;
        }

        if (code[in] == ',') {
            ops.push_back(Op(OpType::Input));
            in++;
            continue;
        }

        if (code[in] == '.') {
            ops.push_back(Op(OpType::Output));
            in++;
            continue;
        }

        error("Invalid command " + std::string(1, code[in]));
    }

    compute_jumps();
}

void BFVM::compute_jumps() {
    // comppute the jumps
    jumps.clear();
    jumps.resize(ops.size(), -1);
    std::vector<int> stack;
    stack.reserve(ops.size());
    for (int i = 0; i < static_cast<int>(ops.size()); i++) {
        if (ops[i].type == OpType::StartLoop) {
            stack.push_back(i);
        }
        else if (ops[i].type == OpType::EndLoop) {
            if (stack.empty()) {
                error("Unmatched ']'");
            }
            int open = stack.back();
            stack.pop_back();
            jumps[open] = i;
            jumps[i] = open;
        }
    }
    if (!stack.empty()) {
        error("Unmatched '['");
    }
}

void BFVM::dump_state() const {
    // Find the last non-zero cell
    int last_nz = 0;
    for (int i = static_cast<int>(tape.size()); i-- > 0; ) {
        if (tape[i] != 0) {
            last_nz = i;
            break;
        }
    }

    // Ensure we still show the pointer cell even if it's beyond last_nz
    int last_to_show = std::max(last_nz, ptr);
    if (last_to_show >= static_cast<int>(tape.size())) {
        last_to_show = static_cast<int>(tape.size()) - 1;
    }

    std::cout << "Tape:";
    for (int i = 0; i <= last_to_show; ++i) {
        std::cout << std::setw(3) << static_cast<int>(tape[i]) << ' ';
    }
    std::cout << "\n     ";
    if (ptr > 0) {
        std::cout << std::string(ptr * 4, ' ');
    }
    std::cout << "^^^" << " (ptr=" << ptr << ")\n\n";
}

void BFVM::run() {
    pc = ptr = 0;
    tape.clear();
    tape.push_back(0); // initialize tape with one cell

    while (pc < static_cast<int>(ops.size())) {
        if (trace) {
            std::cout << "PC=" << pc << " instr=" << ops[pc].to_string() << "\n";
        }

        switch (ops[pc].type) {
        case OpType::Move:
            ptr += ops[pc].value;
            if (ptr < 0) {
                error("Tape pointer underflow");
            }
            else if (ptr >= static_cast<int>(tape.size())) {
                tape.resize(ptr + 1, 0);
            }
            break;

        case OpType::Clear:
            tape[ptr] = 0;
            break;

        case OpType::Increment:
            tape[ptr] += static_cast<uint8_t>(ops[pc].value);
            break;

        case OpType::Multiply: {
            uint8_t origin_val = tape[ptr];
            if (origin_val == 0) {
                break;    // nothing to do
            }

            for (const auto& target : ops[pc].targets) {
                int target_ptr = ptr + target.offset;
                if (target_ptr < 0) {
                    error("Tape pointer underflow");
                }
                if (target_ptr >= static_cast<int>(tape.size())) {
                    tape.resize(target_ptr + 1, 0);
                }

                tape[target_ptr] = static_cast<uint8_t>(tape[target_ptr] +
                                                        (origin_val * target.factor));
            }

            tape[ptr] = 0;
            break;
        }
        case OpType::Scan: {
            int direction = ops[pc].value;
            while (ptr >= 0 && ptr < static_cast<int>(tape.size()) && tape[ptr] != 0) {
                ptr += direction;
            }

            // Se o ponteiro sair dos limites, expandimos a tape conforme necessário
            if (ptr < 0) {
                error("Tape pointer underflow");
            }
            else if (ptr >= static_cast<int>(tape.size())) {
                tape.resize(ptr + 1, 0);
            }
            break;
        }
        case OpType::StartLoop:
            if (tape[ptr] == 0) {
                pc = jumps[pc];
                if (pc < 0) {
                    error("Invalid jump");
                }
                continue;      // do not pc++ below
            }
            break;

        case OpType::EndLoop:
            if (tape[ptr] != 0) {
                pc = jumps[pc];
                if (pc < 0) {
                    error("Invalid jump");
                }
                continue;      // do not pc++ below
            }
            break;

        case OpType::Input:
            tape[ptr] = static_cast<uint8_t>(std::cin.get());
            break;

        case OpType::Output:
            std::cout.put(static_cast<char>(tape[ptr]));
            break;

        default:
            error("Invalid op");
        }

        if (trace) {
            dump_state();
        }

        pc++;
    }
}

int main(int argc, char* argv[]) {
    BFVM vm;
    bool dump_after = false;
    const char* filename = nullptr;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "-t") == 0) {
            vm.set_trace(true);
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
        vm.read_code(std::cin);
    }
    else {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            error("Cannot open file: " + std::string(filename));
        }
        vm.read_code(file);
    }

    vm.compile_code();
    vm.run();

    if (dump_after) {
        vm.dump_state();
    }

    return 0;
}
