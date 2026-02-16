#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# question mark shows usage
capture_nok("bfpp -?", <<END);
usage: bfpp [-o output_file] [-I include_path] [-D name=value] [-v] [input_file]
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
+++[->+<],+.
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
+++---
END

# test continuation lines
spew("$test.in", <<END);
+++\\
---\\
>>> +
<<< -
END
capture_ok("bfpp $test.in", <<END);
+++--->>>+<<<-
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
+++++---------++++
END

# test abolute position after >/<
spew("$test.in", <<END);
>0 +
>4 +
>4 +
>0 +
END
capture_ok("bfpp $test.in", <<END);
+>>>>++<<<<+
END

spew("$test.in", <<END);
<8 +
<8 +
<4 +
<8 +
END
capture_ok("bfpp $test.in", <<END);
>>>>>>>>++<<<<+>>>>+
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
++++---->>>>+<<<<+>>>>+
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
----++++>>>>+<<<<+
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
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.[-]++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+++++++++++++++++.[-]+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+++++++++++++++++++++++++++++++++++++++++++++++++.[-]+++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+.[-]+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++.[-]
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

unlink_testfiles;
done_testing;
