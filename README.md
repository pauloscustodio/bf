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
- Allocation: `alloc_cellX(NAME)`, `free_cellX(NAME)`. (X=`8`, `16`).
- Cell ops: `clearX(a)`, `setX(a, v)`, `moveX(a, b)`, `copyX(a, b)`. (X=`8`, `16`).
- Logic: `notX(a)`, `andX(a, b)`, `orX(a, b)`, `xorX(a, b)`, `shrX(a, b)`, `shlX(a, b)`. (X=`8`, `16`).
- Arithmetic: `addX(a, b)`, `subX(a, b)`, `mulX(a, b)`, `divX(a, b)`, `modX(a, b)`, `powX(a, b)`. (X=`8`, `8s`, `16`, `16s`).
  `negX(a)`, `signX(a)`, `absX(a)`. (X=`8`, `16`).
- Comparisons: `eqX(a, b)`, `neX(a, b)`, `ltX(a, b)`, `leX(a, b)`, `gtX(a, b)`, `geX(a, b)`, `minX(a, b)`, `maxX(a, b)`. (X=`8`, `8s`, `16`, `16s`).
- Control: `if(expr) ... else ... endif`, `while(expr) ... endwhile`, `repeat(count) ... endrepeat`.
- Arrays: `alloc_arrayX(NAME, size)`, `free_arrayX(NAME)`, `put_arrayX(NAME, idx_cell, source_cell)`, `get_arrayX(NAME, idx_cell, target_cell)`. (X=`8`, `16`).
- Strings: allocated with `alloc_array8`, item 0 holds length, item 1 first character.
  `set_string(STR, "string")`, `clear_string(STR)`, `append_string(DST, SRC)`, `left_string(DST, SRC, size)`, `right_string(DST, SRC, size)`, `mid_string(DST, SRC, start, size)`, `cmp_string(STR1, STR2, result)`.
  `format_cellX(DST, cell)`, `string_valX(SRC, cell)`. (X=`8`, `8s`, `16`, `16s`).
- Output: `print_char(value)`, `print_char8(cell)`.  
  `print_string("string")`. string supports common C escape sequences: `\n`, `\t`, `\r`, `\\`, `\"`, `\'`, `\0`, `\a`, `\b`, `\f`, `\v`.
  `print_string(STR)`, `print_newline`.
  `print_cellX(cell)`. (X=`8`, `8s`, `16`, `16s`).
- Input: `scan_spaces`, `scan_char8(cell)`, `unscan_char8(cell)`.  
  `scan_cellX(cell)`. (X=`8`, `8s`, `16`, `16s`).
  `scan_word(STR)`.

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
- `DIM var(size)` - allocates array, `A(i)` accesses element, 1 <= i <= size.
- `INPUT var1 [, var2]` - read integers into `varN`.
- `PRINT [string|expr][,|;]...` - print values of expressions and strings
- `IF expr THEN stmts [ELSE stmts]` - single line if-else.
- `IF expr THEN`, ..., `[ELSE`, ..., `]ENDIF` - multi-line if-else.
- `WHILE expr`, ..., `WEND` - loops controled on an expression.
- `FOR var = expr TO expr [STEP expr]`, ..., `NEXT` - counted loops.
- a colon (`:`) can be used to separate statements in the same line.
- a single `_` followed by newline is considered white space and ignored.

Strings:
- Must be DIM-ensinoned to the maximum size - `DIM A$(10)`.
- Can be assigned - `[LET] A$ = "hello"`.
- Can be concatenated - `[LET] A$ = B$ & C$`.
- String functions in expressions - `LEFT$`, `MID$`, `RIGHT$`, `STR$`, `LEN`, `VAL`, `CHR$`, `ASC`.

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

