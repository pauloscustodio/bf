#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# push8 - error no arguments
spew("$test.in", "push8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected '(' after macro name 'push8'
END

# push8 - error empty arguments
spew("$test.in", "push8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: macro 'push8' expects 1 argument
END

# push8 - error too many arguments
spew("$test.in", "push8(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# push8 - copies cell to stack, decrements stack pointer
spew("$test.in", "alloc_cell8(A) set8(A, 127) push8(A) set8(A, 63) push8(A)");
capture_ok("bfpp $test.in", <<END);
[-]+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++>[-]>>>[-]<<<<[->>>>+<<<+<]>[-
<+>][-]<[-]+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++>[-]>[
-]<<[->>+<+<]>[-<+>][-]<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape: 63   0  63   0 127 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
