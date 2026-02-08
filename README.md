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
- Allocation: `alloc_cell(NAME)`, `alloc_cell16(NAME)`, `free_cell(NAME)`, `free_cell16(NAME)`.
- Cell ops: `clear(a)`, `clear16(a)`, `set(a, v)`, `set16(a, v)`, `move(a, b)`, `move16(a, b)`, `copy(a, b)`, `copy16(a, b)`.
- Logic: `not(a)`, `and(a, b)`, `or(a, b)`, `xor(a, b)`, `shr(a, b)`, `shl(a, b)` (and `xx16` forms).
- Arithmetic: `add/sub/mul/div/mod(a, b)` (+ `xx16`).
- Comparisons: `eq/ne/lt/le/gt/ge(a, b)` (+ `xx16`).
- Control: `if(expr) ... else ... endif`, `while(expr) ... endwhile`, `repeat(count) ... endrepeat`.

Notes:
- Allocation reserves cells from 0 upward and zeroes them.
- `move` zeroes the source; `copy` preserves it.
- Division/mod are integer; arithmetic wraps at 8-bit (or 16-bit for `xx16`).

## Building

Makefile project targeting C++17. Typical flow:

```
git clone <repo_url>
cd <repo_dir>
make
```

This produces `bf` and `bfpp` executables in the project directory. You can run them as described above.
