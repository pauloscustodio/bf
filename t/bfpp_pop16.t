#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# pop16 - error no arguments
spew("$test.in", "pop16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected '(' after macro name 'pop16'
END

# pop16 - error empty arguments
spew("$test.in", "pop16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: macro 'pop16' expects 1 argument
END

# pop16 - error too many arguments
spew("$test.in", "pop16(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# pop16 - copies cell pair to stack, decrements stack pointer
spew("$test.in", <<END);
	alloc_cell16(A)
	alloc_cell16(B)
	push16i(511)
	push16i(1023)
	pop16(A)
	pop16(B)
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]>[-]>[-]>>>[-]+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++>[-]+<<<[-]+++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++>[-]+++<<<<<[-]>>>
>[-<<<<+>>>>]<<<[-]>>>>[-<<<<+>>>>]<<<[-]>>>>[-<<<<+>>>>]<<<[-]>>>>[-<<<<+>>>>]<
<<<<<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:255   3 255   1 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
