#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# question mark shows usage
capture_nok("bfpp -?", <<END);
usage: bfpp [-o output_file] [-I include_path] [-D name=value] [input_file]
END

# use as a filter
spew("$test.in", ">>++");
capture_ok("bfpp < $test.in", <<END);
>>++
END

# use with an input file
spew("$test.in", ">>++");
capture_ok("bfpp $test.in", <<END);
>>++
END

# use with an output file
spew("$test.in", ">>++");
capture_ok("bfpp -o $test.out $test.in", "");
check_text_file("$test.out", <<END);
>>++
END

# test all BF commands
spew("$test.in", "+++[->+<],+.");
capture_ok("bfpp -o $test.out $test.in", "");
check_text_file("$test.out", <<END);
+++
[
  ->+<
]
,+.
END

# test single line comments
spew("$test.in", "+++//this is a comment with bf chars +-<>[].,");
capture_ok("bfpp $test.in", <<END);
+++
END

# test multi-line comments
spew("$test.in", <<END);
+++/*
this is a comment with bf chars +-<>[].,
and with ***
*/---
END
capture_ok("bfpp $test.in", <<END);
+++


---
END

# test continuation lines
spew("$test.in", <<END);
+++\\
---\\
>>> +
<<< -
END
capture_ok("bfpp $test.in", <<END);
+++--->>>+


<<<-
END

# test error on negative tape index
spew("$test.in", "<");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: tape pointer moved to negative position
END

# test error with unbalanced []
spew("$test.in", "[[[");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: unmatched '[' instruction
$test.in:1:2: error: unmatched '[' instruction
$test.in:1:3: error: unmatched '[' instruction
END

spew("$test.in", "]]]");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: unmatched ']' instruction
$test.in:1:2: error: unmatched ']' instruction
$test.in:1:3: error: unmatched ']' instruction
END

# test count after -/+
spew("$test.in", <<END);
+0
+
+4
+(-4)
-0
-
-4
-(-4)
END
capture_ok("bfpp $test.in", <<END);

+
++++
----

-
----
++++
END

# test abolute position after >/<
spew("$test.in", <<END);
>0 +
>4 +
>4 +
>0 +
END
capture_ok("bfpp $test.in", <<END);
+
>>>>+
+
<<<<+
END

spew("$test.in", <<END);
<8 +
<8 +
<4 +
<8 +
END
capture_ok("bfpp $test.in", <<END);
>>>>>>>>+
+
<<<<+
>>>>+
END

# test undefined symbols after <>+-
spew("$test.in", "+X-X>X<X");
capture_nok("bfpp $test.in", <<END);
$test.in:1:2: error: macro 'X' is not defined
$test.in:1:4: error: macro 'X' is not defined
$test.in:1:6: error: macro 'X' is not defined
$test.in:1:8: error: macro 'X' is not defined
END

# test symbols after <>-+
spew("$test.in", <<END);
+X
-X
>X+
>0+
<X+
END
capture_ok("bfpp -DX=4 $test.in", <<END);
++++
----
>>>>+
<<<<+
>>>>+
END

spew("$test.in", <<END);
+X
-X
>X
<X
END
capture_ok("bfpp -DX=0 $test.in", <<END);
END

# expressions after <>+-
spew("$test.in", <<END);
+(X-8)
-(X-8)
<(Y*(Y))+
>0+
END
capture_ok("bfpp -D X=4 -D Y=2 $test.in", <<END);
----
++++
>>>>+
<<<<+
END

# test invalid names in -D
spew("$test.in", "");
capture_nok("bfpp -Dif $test.in", <<END);
bfpp: macro name is a reserved keyword: if
END

# default -D
spew("$test.in", "+X");
capture_ok("bfpp -D X $test.in", <<END);
+
END

# invalid characters in expression
spew("$test.in", "+(*2)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:3: error: unexpected token in expression
$test.in:1:4: error: expected ')'
$test.in:1:4: error: unexpected token in statement: '2'
$test.in:1:5: error: unexpected token in statement: ')'
END

# invalid characters in code
spew("$test.in", "Hello");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: unexpected token in statement: 'Hello'
END

# add quoted-characters in BF instructions
spew("$test.in", <<END);
+'H'.[-]
+'e'.[-]
+'l'.[-]
+'l'.[-]
+'o'.[-]
END
capture_ok("bfpp $test.in", <<END);
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
[
  -
]
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
[
  -
]
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
[
  -
]
END

# test braces
spew("$test.in", "}");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: unmatched '}' brace
END

spew("$test.in", "{");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: unmatched '{' brace
END

spew("$test.in", "{ >>>>+ }");
capture_ok("bfpp $test.in", <<END);
>>>>+<<<<
END

spew("$test.in", ">>>>+ { <<<<+ }");
capture_ok("bfpp $test.in", <<END);
>>>>+<<<<+>>>>
END

# detect mismatch in tape position between start and end loop
spew("$test.in", "[>]");
capture_nok("bfpp $test.in", <<END);
$test.in:1:3: error: tape pointer mismatch at ']' instruction (expected 0, got 1)
$test.in:1:1: note: corresponding '[' instruction here
END

# test #include errors
spew("$test.in", "#include");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected string literal after #include
END

spew("$test.in", "#include 123");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected string literal after #include
END

spew("$test.inc", "");
spew("$test.in", "#include \"$test.inc\" 123");
capture_nok("bfpp $test.in", <<END);
$test.in:1:28: error: unexpected token after #include: '123'
END

unlink("$test.inc");
spew("$test.in", "#include \"$test.inc\"");
capture_nok("bfpp $test.in", <<END);
$test.in:1:27: error: cannot open file '$test.inc'
END

# test #include files
spew("$test.inc", "+++");
spew("$test.in", <<END);
#include "$test.inc"
>
#include "$test.inc"
>
END
capture_ok("bfpp $test.in", <<END);
+++>+++>
END

# include from -I path
path("$test.dir")->mkpath;
spew("$test.dir/$test.inc", "+++");
spew("$test.in", <<END);
#include "$test.inc"
>
END
capture_ok("bfpp -I $test.dir $test.in", <<END);
+++>
END
path("$test.dir")->remove_tree;

# #define error - no name
spew("$test.in", "#define");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected macro name
END

# #define error - reserved name
spew("$test.in", "#define if 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'if': reserved word
END

# #define error - duplicate parameters
spew("$test.in", <<END);
#define X(A,A) 	\\
	+A
END
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: duplicate parameter name 'A' in macro 'X'
END

# #define error - reserved parameter
spew("$test.in", "#define X(if)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:11: error: cannot define parameter 'if': reserved word
END

# #define error - duplicate macro
spew("$test.in", <<END);
#define X 1
#define X 2
END
capture_nok("bfpp $test.in", <<END);
$test.in:2:9: error: macro 'X' redefined
$test.in:1:9: note: previous definition was here
END

# #define - single-line object macro
spew("$test.in", <<END);
#define A B
#define B C
#define C 4
+A
END
capture_ok("bfpp $test.in", <<END);



++++
END

# #define - multi-line object macro
spew("$test.in", <<END);
#define X	\\
[-]+'H'.
X
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
END

# #define - multi-line function macro
spew("$test.in", <<END);
#define COPY(A,B,T)	\\
{ >B [-] >T [-] >A [ - >B + >T + >A ] >T [ - >A + >T ] }
++++
COPY(0,1,2)
END
capture_ok("bfpp $test.in", <<END);


++++>
[
  -
]
>
[
  -
]
<<
[
  ->+>+<<
]
>>
[
  -<<+>>
]
<<
END

# #undef error - no symbol
spew("$test.in", "#undef");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected macro name
END

# #undef error - reserved word
spew("$test.in", "#undef if");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: cannot undefine reserved word 'if'
END

# #undef error - extra arguments
spew("$test.in", "#undef A B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: unexpected token after #undef: 'B'
END

# #undef
spew("$test.in", <<END);
#define X 2
#undef X
#define X 3
+X
END
capture_ok("bfpp $test.in", <<END);



+++
END

# #if error - no expression
spew("$test.in", <<END);
#if 
END
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: missing expression after #if
END

# #if error - tokens after expression
spew("$test.in", <<END);
#if 1 !
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: unexpected token after #if: '!'
END

# #if error - no #endif
spew("$test.in", <<END);
#if 1
END
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: unterminated #if (missing #endif)
END

# #if error - two #else - if true
spew("$test.in", <<END);
#if 1
#else
#else 
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:3:7: error: multiple #else in the same #if
END

# #if error - two #else - if false
spew("$test.in", <<END);
#if 0
#else
#else 
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:3:7: error: multiple #else in the same #if
END

# #if error - #elsif after #else - if true
spew("$test.in", <<END);
#if 1
#else
#elsif 1
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:3:8: error: #elsif after #else
END

# #if error - #elsif after #else - if false
spew("$test.in", <<END);
#if 0
#else
#elsif 1
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:3:8: error: #elsif after #else
END

# #if - simple
spew("$test.in", <<END);
#if 1
+
#endif
END
capture_ok("bfpp $test.in", <<END);

+
END

# #if - simple
spew("$test.in", <<END);
#if 0
+
#endif
END
capture_ok("bfpp $test.in", <<END);
END

# #if with variable - false
spew("$test.in", <<END);
#if X
+
#endif
END
capture_ok("bfpp $test.in", <<END);
END

# #if with variable - true
spew("$test.in", <<END);
#if X
+
#endif
END
capture_ok("bfpp -D X=1 $test.in", <<END);

+
END

# #if with #else - false
spew("$test.in", <<END);
#if 0
+
#else
-
#endif
END
capture_ok("bfpp $test.in", <<END);



-
END

# #if with #else - true
spew("$test.in", <<END);
#if 1
+
#else
-
#endif
END
capture_ok("bfpp $test.in", <<END);

+
END

# #if - #elsif - #elsif - #else - #endif - take branch 1
spew("$test.in", <<END);
#if 1
+
#elsif 1
++
#elsif 1
+++
#else
++++
#endif
END
capture_ok("bfpp $test.in", <<END);

+
END

# #if - #elsif - #elsif - #else - #endif - take branch 2
spew("$test.in", <<END);
#if 0
+
#elsif 1
++
#elsif 1
+++
#else
++++
#endif
END
capture_ok("bfpp $test.in", <<END);



++
END

# #if - #elsif - #elsif - #else - #endif - take branch 3
spew("$test.in", <<END);
#if 0
+
#elsif 0
++
#elsif 1
+++
#else
++++
#endif
END
capture_ok("bfpp $test.in", <<END);





+++
END

# #if - #elsif - #elsif - #else - #endif - take branch 4
spew("$test.in", <<END);
#if 0
+
#elsif 0
++
#elsif 0
+++
#else
++++
#endif
END
capture_ok("bfpp $test.in", <<END);







++++
END

# alloc_cell - error no arguments
spew("$test.in", "alloc_cell");
capture_nok("bfpp $test.in", <<END);
$test.in:1:11: error: expected '(' after macro name 'alloc_cell'
END

# alloc_cell - error empty arguments
spew("$test.in", "alloc_cell()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: alloc_cell expects one identifier
END

# alloc_cell - error too many arguments
spew("$test.in", "alloc_cell(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: expected ')' at end of macro call, found ','
END

# alloc_cell - wrong type of arguments
spew("$test.in", "alloc_cell(1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: alloc_cell expects one identifier
END

# alloc_cell - use as reserved word
spew("$test.in", "#define alloc_cell 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'alloc_cell': reserved word
END

# alloc_cell
spew("$test.in", <<END);
alloc_cell(A) /* A=0 */
alloc_cell(B) /* B=1 */
>A
>B
>A
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
<
END

# free_cell - error no arguments
spew("$test.in", "free_cell");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected '(' after macro name 'free_cell'
END

# free_cell - error empty arguments
spew("$test.in", "free_cell()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:12: error: free_cell expects one identifier
END

# free_cell - error too many arguments
spew("$test.in", "free_cell(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:12: error: expected ')' at end of macro call, found ','
END

# free_cell - wrong type of arguments
spew("$test.in", "free_cell(1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: free_cell expects one identifier
END

# free_cell - use as reserved word
spew("$test.in", "#define free_cell 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'free_cell': reserved word
END

# free_cell - use after free
spew("$test.in", "alloc_cell(A) free_cell(A) >A");
capture_nok("bfpp $test.in", <<END);
$test.in:1:29: error: macro 'A' is not defined
END

# free_cell
spew("$test.in", <<END);
alloc_cell(A) /* A=0 */
alloc_cell(B) /* B=1 */
alloc_cell(C) /* C=2 */
free_cell(B)
alloc_cell(X) /* X=1 */
>A
>X
>C
>A
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
>
[
  -
]
<
[
  -
]
[
  -
]
<
END

# clear - error no arguments
spew("$test.in", "clear");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected '(' after macro name 'clear'
END

# clear - error empty arguments
spew("$test.in", "clear()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: clear expects one argument
END

# clear - error too many arguments
spew("$test.in", "clear(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# clear - clear the given cells
spew("$test.in", "clear(0) clear(1)");
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
<
END

# clear - execute a test program
spew("$test.in", "++++>++++< clear(0) clear(1)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0 
     ^^^ (ptr=0)

END

# set - error no arguments
spew("$test.in", "set");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'set'
END

# set - error empty arguments
spew("$test.in", "set()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: set expects two arguments
END

# set - error too many arguments
spew("$test.in", "set(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# set - set the given cells
spew("$test.in", "set(0,3) set(1,2)");
capture_ok("bfpp $test.in", <<END);
[
  -
]
+++>
[
  -
]
++<
END

# set - execute a test program
spew("$test.in", "++++ set(0,10)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape: 10 
     ^^^ (ptr=0)

END

# move - error no arguments
spew("$test.in", "move");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'move'
END

# move - error empty arguments
spew("$test.in", "move()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: move expects two arguments
END

# move - error too many arguments
spew("$test.in", "move(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# move - move contents between cells
spew("$test.in", "move(0,1) move(1,0)");
capture_ok("bfpp $test.in", <<END);
>
[
  -
]
<
[
  ->+<
]
[
  -
]
>
[
  -<+>
]
<
END

# move - execute a test program
spew("$test.in", "++++ move(0,1)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   4 
     ^^^ (ptr=0)

END

# copy - error no arguments
spew("$test.in", "copy");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'copy'
END

# copy - error empty arguments
spew("$test.in", "copy()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: copy expects two arguments
END

# copy - error too many arguments
spew("$test.in", "copy(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# copy - copy contents between cells
spew("$test.in", "set(1,4) copy(1,2) copy(1,3)");
capture_ok("bfpp $test.in", <<END);
>
[
  -
]
++++<
[
  -
]
>>
[
  -
]
<
[
  ->+<<+>
]
<
[
  ->+<
]
[
  -
]
[
  -
]
>>>
[
  -
]
<<
[
  ->>+<<<+>
]
<
[
  ->+<
]
[
  -
]
END

# copy - execute a test program
# Note: cell 0 is the temp used by copy
spew("$test.in", ">++++ copy(1,2)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   4   4 
         ^^^ (ptr=1)

END

# not - error no arguments
spew("$test.in", "not");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'not'
END

# not - error empty arguments
spew("$test.in", "not()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: not expects one argument
END

# not - error too many arguments
spew("$test.in", "not(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected ')' at end of macro call, found ','
END

# not - negate the cell value
spew("$test.in", "alloc_cell(X) not(X)");
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<<
END

# not - execute a test program
spew("$test.in", "alloc_cell(X) clear(X) not(X)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1   0   0 
     ^^^ (ptr=0)

END

for my $i (1, 2, 254, 255) {
	spew("$test.in", "alloc_cell(X) set(X, $i) not(X)");
	capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0   0 
     ^^^ (ptr=0)

END
}

# if - error no arguments
spew("$test.in", "if");
capture_nok("bfpp $test.in", <<END);
$test.in:1:3: error: expected '(' after macro name 'if'
END

# if - error empty arguments
spew("$test.in", "if()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: if expects one argument
END

# if - error too many arguments
spew("$test.in", "if(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected ')' at end of macro call, found ','
END

# if - error no endif
spew("$test.in", "if(0)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: if without matching endif
(if):1:99: error: unmatched '[' instruction
(if):1:1: error: unmatched '{' brace
(if):1:101: error: unmatched '{' brace
END

# naked else
spew("$test.in", "else");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: else without matching if
END

# naked endif
spew("$test.in", "endif");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: endif without matching if
END

# if - endif
spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(Y)
set(X, 0)
if(X)
	>Y +
endif
>Y
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
<
[
  -
]
>>
[
  -
]
>
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+>+<<<<
]
>>>>
[
  -<<<<+>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  -
]
>
[
  -<+>>+<
]
>
[
  -<+>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<
[
  <+>-
]
[
  -
]
>
[
  -
]
<<
END

# run if - endif
spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(Y)
set(X, 0)
if(X)
	>Y +
endif
>Y
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0   0   0   0   0 
         ^^^ (ptr=1)

END

spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(Y)
set(X, 123)
if(X)
	>Y +
endif
>Y
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:123   1   0   0   0   0 
         ^^^ (ptr=1)

END

# if - else - endif
spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(Y)
alloc_cell(Z)
set(X, 0)
if(X)
	>Y +
else
	>Z +
endif
>Y
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
>>>
[
  -
]
>
[
  -
]
>
[
  -
]
<
[
  -
]
<<<<
[
  ->>>>+>+<<<<<
]
>>>>>
[
  -<<<<<+>>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  -
]
>
[
  -<+>>+<
]
>
[
  -<+>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<
[
  <<+>>-
]
>
[
  <<+>>-
]
<
[
  -
]
>
[
  -
]
<<<
END

# run if - else - endif
spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(Y)
alloc_cell(Z)
set(X, 0)
if(X)
	>Y +
else
	>Z +
endif
>Z
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0   1   0   0   0   0 
             ^^^ (ptr=2)

END

spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(Y)
alloc_cell(Z)
set(X, 1)
if(X)
	>Y +
else
	>Z +
endif
>Y
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1   1   0   0   0   0   0 
         ^^^ (ptr=1)

END

# while - error no arguments
spew("$test.in", "while");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected '(' after macro name 'while'
END

# while - error empty arguments
spew("$test.in", "while()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: while expects one argument
END

# while - error too many arguments
spew("$test.in", "while(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# while - error no endwhile
spew("$test.in", "while(0)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: while without matching endwhile
(while):1:63: error: unmatched '[' instruction
(while):1:1: error: unmatched '{' brace
(while):1:65: error: unmatched '{' brace
END

# naked endwhile
spew("$test.in", "endwhile");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: endwhile without matching while
END

# run while ... endwhile
spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(COND)
set(COND, 3)
while(COND)
	>X +
	>COND -
endwhile
>X
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
[
  -
]
+++>
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+>+<<
]
>>
[
  -<<+>>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<<
[
  <<+>->>
  [
    -
  ]
  <
  [
    -
  ]
  <
  [
    ->+>+<<
  ]
  >>
  [
    -<<+>>
  ]
  [
    -
  ]
  [
    -
  ]
  >
  [
    -
  ]
  <
  [
    -
  ]
  <
  [
    ->+<
  ]
  +>>+<
  [
    ->
    [
      -<<->>
    ]
    <
  ]
  [
    -
  ]
  >
  [
    -
  ]
  <
  [
    -
  ]
  >
  [
    -
  ]
  <
  [
    -
  ]
  <
  [
    ->+<
  ]
  +>>+<
  [
    ->
    [
      -<<->>
    ]
    <
  ]
  [
    -
  ]
  >
  [
    -
  ]
  <<
]
[
  -
]
<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  3   0   0   0   0 
     ^^^ (ptr=0)

END

# repeat - error no arguments
spew("$test.in", "repeat");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected '(' after macro name 'repeat'
END

# repeat - error empty arguments
spew("$test.in", "repeat()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: repeat expects one argument
END

# repeat - error too many arguments
spew("$test.in", "repeat(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# repeat - error no endrepeat
spew("$test.in", "repeat(0)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: repeat without matching endrepeat
(repeat):1:6: error: unmatched '[' instruction
(repeat):1:1: error: unmatched '{' brace
(repeat):1:8: error: unmatched '{' brace
END

# naked endrepeat
spew("$test.in", "endrepeat");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: endrepeat without matching repeat
END

# run repeat ... endrepeat
spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(COUNT)
set(COUNT, 3)
repeat(COUNT)
	>X +
endrepeat
>X
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
[
  -
]
+++
[
  <+>-
]
<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  3   0 
     ^^^ (ptr=0)

END

# and - error no arguments
spew("$test.in", "and");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'and'
END

# and - error empty arguments
spew("$test.in", "and()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: and expects two arguments
END

# and - error too many arguments
spew("$test.in", "and(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# and(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
and(A,B)
>A
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
>
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
<<
[
  ->>+<<
]
>>>>>
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  -
]
<<
[
  ->>+>>+<<<<
]
>>>>
[
  -<<<<+>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<<
[
  ->>
  [
    -
  ]
  <
  [
    ->+<
  ]
  <
]
<<
[
  -
]
>>>>
[
  -<<<<+>>>>
]
<<
[
  -
]
>
[
  -
]
>
[
  -
]
<<<<
END

# run and(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		and(A, B)
		>A
END
		my $R = ($A && $B) ? 1 : 0;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B   0   0   0   0   0 
     ^^^ (ptr=0)

END
	}
}

# or - error no arguments
spew("$test.in", "or");
capture_nok("bfpp $test.in", <<END);
$test.in:1:3: error: expected '(' after macro name 'or'
END

# or - error empty arguments
spew("$test.in", "or()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: or expects two arguments
END

# or - error too many arguments
spew("$test.in", "or(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected ')' at end of macro call, found ','
END

# or(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
or(A,B)
>A
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
>
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
<<
[
  ->>+<<
]
>>>>>
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  -
]
<<
[
  ->>+>>+<<<<
]
>>>>
[
  -<<<<+>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<<
[
  ->>+<<
]
>
[
  ->+<
]
>>
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<<<<
[
  -
]
>>>>
[
  -<<<<+>>>>
]
<<
[
  -
]
>
[
  -
]
>
[
  -
]
<<<<
END

# run or(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		or(A, B)
		>A
END
		my $R = ($A || $B) ? 1 : 0;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B   0   0   0   0   0 
     ^^^ (ptr=0)

END
	}
}

# xor - error no arguments
spew("$test.in", "xor");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'xor'
END

# xor - error empty arguments
spew("$test.in", "xor()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: xor expects two arguments
END

# xor - error too many arguments
spew("$test.in", "xor(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# xor(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
xor(A,B)
>A
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
>
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
<<
[
  ->>+>>+<<<<
]
>>>>
[
  -<<<<+>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
<<
[
  ->>+<<
]
>>>>>
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  -
]
<<<<
[
  ->>>>+>>+<<<<<<
]
>>>>>>
[
  -<<<<<<+>>>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<<
[
  ->>+<<
]
>
[
  ->+<
]
>>
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<<<<
[
  -
]
>>>>
[
  -<<<<+>>>>
]
<<
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
<
[
  -
]
<<<
[
  ->>>+>+<<<<
]
>>>>
[
  -<<<<+>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
<
[
  ->+<
]
>>>>
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  -
]
<<<<
[
  ->>>>+>>+<<<<<<
]
>>>>>>
[
  -<<<<<<+>>>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<<
[
  ->>
  [
    -
  ]
  <
  [
    ->+<
  ]
  <
]
<
[
  -
]
>>>
[
  -<<<+>>>
]
<<
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<<<
[
  -
]
>>
[
  -<<+>>>>+<<
]
>>
[
  -<<+>>
]
[
  -
]
[
  -
]
>
[
  -
]
>
[
  -
]
<<
[
  -
]
<<<<
[
  ->>>>+<<<<
]
>>>>>>>
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  -
]
<<
[
  ->>+>>+<<<<
]
>>>>
[
  -<<<<+>>>>
]
[
  -
]
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<
[
  -
]
>
[
  -
]
<
[
  -
]
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<<
[
  ->>
  [
    -
  ]
  <
  [
    ->+<
  ]
  <
]
<<<<
[
  -
]
>>>>>>
[
  -<<<<<<+>>>>>>
]
<<
[
  -
]
>
[
  -
]
>
[
  -
]
<<<<
[
  -
]
>
[
  -
]
<<<
END

# run xor(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		xor(A, B)
		>A
END
		my $R = (!!$A != !!$B) ? 1 : 0;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B   0   0   0   0   0   0   0 
     ^^^ (ptr=0)

END
	}
}

unlink_testfiles;
done_testing;
