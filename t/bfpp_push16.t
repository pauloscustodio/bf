#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# push16 - error no arguments
spew("$test.in", "push16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected '(' after macro name 'push16'
END

# push16 - error empty arguments
spew("$test.in", "push16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: macro 'push16' expects 1 argument
END

# push16 - error too many arguments
spew("$test.in", "push16(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# push16 - copies cell pair to stack, decrements stack pointer
spew("$test.in", "alloc_cell16(A) set16(A, 511) push16(A) set16(A, 1023) push16(A)");
capture_ok("bfpp $test.in", <<END);
[-]>[-]<[-]+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++>[-]+>[-]>>>[-]<<<<<[->>>>>+<<<+<<]>>[-<<+>>][-]>>>>[-
]<<<<<[->>>>>+<<<<+<]>[-<+>][-]<<[-]++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+++++++++++++++++++++++++++++++++++++++++++++++++++>[-]+++>[-]>[-]<<<[->>>+<+<<]
>>[-<<+>>][-]>>[-]<<<[->>>+<<+<]>[-<+>][-]<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:255   3   0 255   3 255   1 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
