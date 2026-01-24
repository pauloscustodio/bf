#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# question mark shows usage
capture_nok("bfpp -?", <<'END');
error: Usage: bfpp [-o output_file] [-D name=value] [input_file]
END

# use as a filter
spew("$test.in", ">>++");
capture_ok("bfpp < $test.in", <<'END');
>>++
END

# use with an input file
spew("$test.in", ">>++");
capture_ok("bfpp $test.in", <<'END');
>>++
END

# use with an output file
spew("$test.in", ">>++");
capture_ok("bfpp -o $test.out $test.in", "");
check_text_file("$test.out", <<'END');
>>++
END

# test all BF commands
spew("$test.in", "+++[->+<],+.");
capture_ok("bfpp -o $test.out $test.in", "");
check_text_file("$test.out", <<'END');
+++
[
  ->+<
]
,+.
END

# test single line comments
spew("$test.in", "+++//this is a comment with bf chars +-<>[].,");
capture_ok("bfpp $test.in", <<'END');
+++
END

# test multi-line comments
spew("$test.in", <<'END');
+++/*
this is a comment with bf chars +-<>[].,
and with ***
*/---
END
capture_ok("bfpp $test.in", <<'END');
+++


---
END

# test error on negative tape index
spew("$test.in", "<");
capture_nok("bfpp $test.in", <<'END');
test_t_bfpp.in:1:1: error: tape pointer moved to negative position
END

# test error with unbalanced []
spew("$test.in", "[[[");
capture_nok("bfpp $test.in", <<'END');
test_t_bfpp.in:1:1: error: unmatched '[' instruction
test_t_bfpp.in:1:2: error: unmatched '[' instruction
test_t_bfpp.in:1:3: error: unmatched '[' instruction
END

spew("$test.in", "]]]");
capture_nok("bfpp $test.in", <<'END');
test_t_bfpp.in:1:1: error: unmatched ']' instruction
test_t_bfpp.in:1:2: error: unmatched ']' instruction
test_t_bfpp.in:1:3: error: unmatched ']' instruction
END

# test numbers after <>-+
spew("$test.in", <<'END');
+4+0
-4-0
>4>0
<4<0
END
capture_ok("bfpp $test.in", <<'END');
++++
----
>>>>
<<<<
END

# test undefined symbols after <>+-
spew("$test.in", "+X-X>X<X");
capture_nok("bfpp $test.in", <<'END');
test_t_bfpp.in:1:2: error: macro 'X' is not defined
test_t_bfpp.in:1:4: error: macro 'X' is not defined
test_t_bfpp.in:1:6: error: macro 'X' is not defined
test_t_bfpp.in:1:8: error: macro 'X' is not defined
END

# test symbols after <>-+
spew("$test.in", <<'END');
+X
-X
>X
<X
END
capture_ok("bfpp -DX=4 $test.in", <<'END');
++++
----
>>>>
<<<<
END

spew("$test.in", <<'END');
+X
-X
>X
<X
END
capture_ok("bfpp -DX=0 $test.in", <<'END');
END

# expressions after <>+-
spew("$test.in", <<'END');
+(X-8)
-(X-8)
<(Y*(-Y))
>(-Y*Y)
END
capture_ok("bfpp -D X=4 -D Y=2 $test.in", <<'END');
----
++++
>>>>
<<<<
END

# invalid characters in expression
spew("$test.in", "+(*2)");
capture_nok("bfpp $test.in", <<'END');
test_t_bfpp.in:1:3: error: unexpected token in expression
test_t_bfpp.in:1:4: error: expected ')'
test_t_bfpp.in:1:4: error: unexpected token in statement: '2'
test_t_bfpp.in:1:5: error: unexpected token in statement: ')'
END

# invalid characters in code
spew("$test.in", "Hello");
capture_nok("bfpp $test.in", <<'END');
test_t_bfpp.in:1:1: error: unexpected token in statement: 'Hello'
END


unlink_testfiles;
done_testing;
