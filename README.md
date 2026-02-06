# bf

Brainfuck interpreter

usage: bf [-t] [-D] [input_file]

- -t : Trace execution to stdout

- -D : Dump final status of the machine to stdout

- input_file : parse input file instead of stdin

# bfpp

Brainfuck preprocessor - expands macros and outputs plain BF code.

usage: bfpp [-o output_file] [-I include_path] [-D name=value] [input_file]

- -o output_file : outputs BF code to given file instead of stdout

- -I include_path : add directory to search path for source and include files

- -D name=value : defines numeric macro to be used in the code

- input_file : parse input file instead of stdin

Extended BF code translated to plain BF code:

- [<>] number : move the tape to the given abolute tape location, starting at zero

- [-+] number : decrement/increment the current cell by the given number of times

- [<>-+] name : expands the named macro and evaluates the resulting integer expression; the macro may refer to other macros that are expanded recursively

- [<>-+] '(' expr ')' : evaluate the given integer expression with all the valid C operators; the expression may refer to macros that are expanded recursively

- +'c' : increment the cell by the ASCII code of the quoted character

- '{' : remember the current tape postion

- '}' : emit > or < instructions to return to the tape position remebered by {

Directives:

- #include "file" : searches for file in the include path and inserts it in the input stream

- #define name value : define object like macro

- #define name(param,...) value : define function like macro

- #undef name : undefine macro

- #if expr / #elsif expr / ... / #else / #endif : conditionally select one branch of code

Built-in macros:

- alloc_cell(NAME) : reserves one cell on the tape, creates a macro NAME with it's address; cells are reserved from 0 upwards; clears the resrved cell

- free_cell(NAME) : frees a cell reserved by alloc_cell() and removes the macro NAME; clears the freed cell;

- clear(expr) : clears the cell pointed by the expression

- set(expr1, expr2) : sets the cell pointed by expr1 to the value of expr2

- move(expr1, expr2) : move the value of cell at expr1 to cell at expr2, cell at expr1 is zeroed

- copy(expr1, expr2) : copy the value of cell at expr1 to cell at expr2, uses a temporary allocated from the start of the tape and releases it

- not(expr) : sets cell pointed by expr to 0 if >0, or to 1 if 0

- and(a, b) : sets cell pointed by a to 1 if both cells pointed by a and b are non-zero, else sets a to 0; preserves b

- or(a, b) : sets cell pointed by a to 1 if any of the cells pointed by a and b are non-zero, else sets a to 0; preserves b

- xor(a, b) : sets cell pointed by a to 1 if any of the cells pointed by a and b are non-zero, but not both, else sets a to 0; preserves b

- add(a, b) : sets a to a+b, wraps around on 8-bit overflow; preserves b

- sub(a, b) : sets a to a-b, wraps around on 8-bit overflow; preserves b

- mul(a, b) : sets a to a\*b, wraps around on 8-bit overflow; preserves b

- div(a, b) : sets a to int(a/b); preserves b

- mod(a, b) : sets a to a%b; preserves b

- eq(a, b) : sets a to 1 if a==b, to 0 otherwise; preserves b

- ne(a, b) : sets a to 1 if a!=b, to 0 otherwise; preserves b

- lt(a, b) : sets a to 1 if a<b, to 0 otherwise; preserves b

- le(a, b) : sets a to 1 if a<=b, to 0 otherwise; preserves b

- gt(a, b) : sets a to 1 if a>b, to 0 otherwise; preserves b

- ge(a, b) : sets a to 1 if a>=b, to 0 otherwise; preserves b

- if(expr) ... else ... endif : if the value of cell at expr is non-zero, execute the code up to the else, else execute the code between else and endif

- while(expr) ... endwhile : repeat the loop while the value of the cell at expr is non-zero

- repeat(count) ... endrepeat : repeats block while count != 0, decrements count to zero
