# bf, bfpp & bfbasic

Brainfuck tools: a tiny interpreter (`bf`), a macro preprocessor (`bfpp`) that emits plain BF, and a tiny BASIC compiler (`bfbasic`) that emits bfpp macros.

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
- -v : show memory usage statistics

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
- Arithmetic: `add8/add8s/sub8/sub8s/mul8/mul8s/div8/div8s/mod8/mod8s/pow8/pow8s(a, b)`, `neg8/sign8/abs8(a)` (+ `xx16`).
- Comparisons: `eq8/eq8s/ne8/ne8s/lt8/lt8s/le8/le8s/gt8/gt8s/ge8/ge8s(a, b)` (+ `xx16`).
- Control: `if(expr) ... else ... endif`, `while(expr) ... endwhile`, `repeat(count) ... endrepeat`.
- Stack: `push8(source_cell)`, `push8i(immediate_value)`, `pop8(target_cell)` (+ `xx16`).
- Global: `alloc_global16(COUNT)`, `free_global16`, `alloc_temp16(COUNT)`, `free_temp16`. Exressions can use `global(i)` and `temp(i)` to refer to these.
- Stack frames: `enter_frame16(num_args16, num_locals16)` / `leave_frame16` to manage a call stack for temporary storage. `num_args16` are already on the stack when `enter_frame16` is called. `frame_alloc_temp16(count16)` can be used inside a frame to allocate temporary cells that will be automatically freed on `leave_frame16`. Expressions can use `arg(i)` to refer to arguments,  `local(i)` for local variables and `frame_temp(i)` for temporaries.
- Arrays: `alloc_array16(NAME, dimensions)`, `free_array16(NAME)`, `put_array8(NAME, idx_cell, source_cell)`, `get_array8(NAME, idx_cell, target_cell)` (and `xx16` forms for put/get).
- Output: `print_char(value)`, `print_char8(cell)`, `print_string(string)`; string supports common C escape sequences: `\n`, `\t`, `\r`, `\\`, `\"`, `\'`, `\0`, `\a`, `\b`, `\f`, `\v`;
  `print_newline`, `print_cell8(cell)`, `print_cell16(cell)`, `print_cell8s(cell)`, `print_cell16s(cell)`.
- Input: `scan_spaces`, `scan_char8(cell)`, `unscan_char8(cell)`, `scan_cell8(cell)`, `scan_cell8s(cell)`, `scan_cell16(cell)`, `scan_cell16s(cell)`.

Notes:
- Allocation reserves cells from 0 upward and zeroes them.
- `move` zeroes the source; `copy` preserves it.
- Division/modulo are integer; arithmetic wraps at 8-bit (or 16-bit for `xx16`).

## bfbasic - BASIC to bfpp compiler

Compiles a tiny BASIC subset to Brainfuck (via `bfpp` codegen).

usage: bfbasic [-o output_file] input_file
- -o output_file : write output to the given file instead of stdout
- input_file : BASIC source file (reads stdin if omitted)

Supported statements (one per line; blank lines allowed):
- `[LET] var = expr` - assign integer expression to a variable.
- `INPUT var1 [, var2]` - read integers into `varN`.
- `PRINT [string|expr][,|;]...` - print values of expressions and strings
- `IF expr THEN stmts [ELSE stmts]` - single line if-else.
- `IF expr THEN`, ..., `[ELSE`, ..., `]ENDIF` - multi-line if-else.
- a colon (`:`) can be used to separate statements in the same line.
- a single `_` followed by newline is considered white space and ignored.

Expressions:
- variables and decimal integers
- binary arithmetic: `+  -  *  /  \\  MOD  ^`
- relational: `=  <>  <  <=  >  >=` (results are 0/1)
- boolean logic: `AND  OR  XOR` (operates on 0/1)

## Expression operators and precedence
Precedence and associativity (highest to lowest):
1. unary (`+`, `-`, `NOT`)
2. power (`^`)
3. multiplicative (`*`, `/`, `\`, `MOD`)
4. additive (`+`, `-`)
5. shift (`SHL`, `SHR`)
6. relational (`=`, `<>`, `<`, `<=`, `>`, `>=`)
7. logical AND (`AND`)
8. logical XOR (`XOR`)
9. logical OR (`OR`)


Parentheses `(...)` can be used to override precedence.
The compiler allocates variables automatically and emits `bfpp` code.

## Building

Makefile project targeting C++17. Typical flow:

```
git clone <repo_url>
cd <repo_dir>
make
make test
```

This produces `bf`, `bfpp` and `bfbasic` executables in the project directory. You can run them as described above.

## Example

Here's a simple example using `bfpp` to create a BF program that prints "Hello, World!":

```
./bf examples/hello.bf
./bfpp examples/hello.bfpp | ./bf
```

Here's a simple calculator written in `bfpp` and compiled to `bf`:

```
echo '2*3+7' | ./bf examples/calc.bf
```

## License

This project is licensed under the [Artistic License 2.0](http://www.perlfoundation.org/artistic_license_2_0).

