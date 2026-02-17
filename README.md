# bf & bfpp

Brainfuck tools: a tiny interpreter (`bf`) and a macro preprocessor (`bfpp`) that emits plain BF.

## bf - interpreter

usage: bf [-t] [-D] [input_file]
- -t : Trace execution to stdout
- -D : Dump final status of the machine to stdout
- input_file : parse input file instead of stdin

Reads `input_file` or stdin, processes only canonical BF chars (`<>+-.,[]`). Tape grows right; pointer underflow is an error.

## bfpp - preprocessor

Expands an extended BF dialect to plain BF.

usage: bfpp [-o output_file] [-I include_path] [-D name=value] [-v] [input_file]
- -o output_file : outputs BF code to given file instead of stdout
- -I include_path : add directory to search path for source and include files
- -D name=value : defines numeric macro to be used in the code
- input_file : parse input file instead of stdin
- v : show memory usage statistics

### Extended syntax
- `>N` / `<N` : absolute move to tape cell `N` (0-based).
- `+N` / `-N` : increment/decrement current cell by `N`.
- `+(expr)` / `-(expr)` / `>(expr)` / `<(expr)` : evaluate `expr` (identifiers/macros allowed).
- `+'c'` : increment by ASCII of character `c`.
- `{ ... }` : save/restore tape position (emits moves to return).
- `#include "file"` : include file.
- `#define NAME value` : object-like macro.
- `#define NAME(p,...) value` : function-like macro.
- `#undef NAME`
- `#if / #elsif / #else / #endif` : conditional assembly on expressions.

### Built-in macros (8-bit; `xx16` variants treat two cells little-endian)
- Allocation: `alloc_cell8(NAME)`, `free_cell8(NAME)`. (+ `xx16`).
- Cell ops: `clear8(a)`, `set8(a, v)`, `move8(a, b)`, `copy8(a, b)`. (+ `xx16`).
- Logic: `not8(a)`, `and8(a, b)`, `or8(a, b)`, `xor8(a, b)`, `shr8(a, b)`, `shl8(a, b)` (and `xx16` forms).
- Arithmetic: `add8/add8s/sub8/sub8s/mul8/mul8s/div8/div8s/mod8/mod8s(a, b)`, `neg8/sign8/abs8(a)` (+ `xx16`).
- Comparisons: `eq8/eq8s/ne8/ne8s/lt8/lt8s/le8/le8s/gt8/gt8s/ge8/ge8s(a, b)` (+ `xx16`).
- Control: `if(expr) ... else ... endif`, `while(expr) ... endwhile`, `repeat(count) ... endrepeat`.
- Stack: `push8(source_cell)`, `push8i(immediate_value)`, `pop8(target_cell)` (+ `xx16`).
- Global: `alloc_global16(COUNT)`, `free_global16`, `alloc_temp16(COUNT)`, `free_temp16`. Exressions can use `global(i)` and `temp(i)` to refer to these.
- Stack frames: `enter_frame16(num_args16, num_locals16)` / `leave_frame16` to manage a call stack for temporary storage. `num_args16` are already on the stack when `enter_frame16` is called. `frame_alloc_temp16(count16)` can be used inside a frame to allocate temporary cells that will be automatically freed on `leave_frame16`. Expressions can use `arg(i)` to refer to arguments,  `local(i)` for local variables and `frame_temp(i)` for temporaries.
- Input: `scan_spaces`, `scan_char8(cell)`, `unscan_char8(cell)`, `scan_cell8(cell)`, `scan_cell8s(cell)`, `scan_cell16(cell)`, `scan_cell16s(cell)`.

Notes:
- Allocation reserves cells from 0 upward and zeroes them.
- `move` zeroes the source; `copy` preserves it.
- Division/modulo are integer; arithmetic wraps at 8-bit (or 16-bit for `xx16`).

## Building

Makefile project targeting C++17. Typical flow:

```
git clone <repo_url>
cd <repo_dir>
make
make test
```

This produces `bf` and `bfpp` executables in the project directory. You can run them as described above.

## Example

Here's a simple example using `bfpp` to create a BF program that prints "Hello, World!":

```
./bf examples/hello.bf
./bfpp examples/hello.bfpp | ./bf
echo '2*3+7' | ./bf examples/calc.bf
```
