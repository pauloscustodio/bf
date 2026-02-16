#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# clear16 - error no arguments
spew("$test.in", "clear16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected '(' after macro name 'clear16'
END

# clear16 - error empty arguments
spew("$test.in", "clear16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: macro 'clear16' expects 1 argument
END

# clear16 - error too many arguments
spew("$test.in", "clear16(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected ')' at end of macro call, found ','
END

# clear16 - clear16 the given cells
spew("$test.in", "clear16(0) clear16(2)");
capture_ok("bfpp $test.in", <<END);
[-]>[-]>[-]>[-]<<<
END

# clear16 - execute a test program
spew("$test.in", "++++>++++>++++>++++> clear16(0) clear16(2) >3");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0   0   0 
                 ^^^ (ptr=3)

END

unlink_testfiles;
done_testing;
