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
>>>
<<<
END
capture_ok("bfpp $test.in", <<END);
+++--->>>


<<<
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
>0
>4
>4
>0
END
capture_ok("bfpp $test.in", <<END);

>>>>

<<<<
END

spew("$test.in", <<END);
<8
<8
<4
<8
END
capture_ok("bfpp $test.in", <<END);
>>>>>>>>

<<<<
>>>>
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
>X
>0
<X
END
capture_ok("bfpp -DX=4 $test.in", <<END);
++++
----
>>>>
<<<<
>>>>
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
<(Y*(Y))
>0
END
capture_ok("bfpp -D X=4 -D Y=2 $test.in", <<END);
----
++++
>>>>
<<<<
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

# labeled loops
spew("$test.in", <<END);
>4
+4
>0
[4 - ]
END
capture_ok("bfpp $test.in", <<END);
>>>>
++++
<<<<
>>>>
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

spew("$test.in", "{ >>>> }");
capture_ok("bfpp $test.in", <<END);
>>>><<<<
END

spew("$test.in", ">>>> { <<<< }");
capture_ok("bfpp $test.in", <<END);
>>>><<<<>>>>
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
+++
>+++

>
END

# include from -I path
path("$test.dir")->mkpath;
spew("$test.dir/$test.inc", "+++");
spew("$test.in", <<END);
#include "$test.inc"
>
END
capture_ok("bfpp -I $test.dir $test.in", <<END);
+++
>
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
{ >B [-] >T [-] [A - >B + >T + >A ] [T - >A + >T ] }
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
$test.in:1:11: error: alloc_cell expects one identifier
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
$test.in:1:16: error: alloc_cell expects one identifier
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



>
<
END

# free_cell - error no arguments
spew("$test.in", "free_cell");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected '(' after macro name 'free_cell'
$test.in:1:10: error: free_cell expects one identifier
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
$test.in:1:15: error: free_cell expects one identifier
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






>
>
<<
END



unlink_testfiles;
done_testing;
