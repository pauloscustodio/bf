#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# push16i - error no arguments
spew("$test.in", "push16i");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected '(' after macro name 'push16i'
END

# push16i - error empty arguments
spew("$test.in", "push16i()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: macro 'push16i' expects 1 argument
END

# push16i - error too many arguments
spew("$test.in", "push16i(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected ')' at end of macro call, found ','
END

# push16i - copies immediate to stack, decrements stack pointer
spew("$test.in", "push16i(511) push16i(1023)");
capture_ok("bfpp $test.in", <<END);
>>
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+++++++++++++++>
[
  -
]
+<<<
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+++++++++++++++>
[
  -
]
+++<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:255   3 255   1 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
