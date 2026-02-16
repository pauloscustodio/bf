#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# clear8 - error no arguments
spew("$test.in", "clear8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected '(' after macro name 'clear8'
END

# clear8 - error empty arguments
spew("$test.in", "clear8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: macro 'clear8' expects 1 argument
END

# clear8 - error too many arguments
spew("$test.in", "clear8(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# clear8 - clear the given cells
spew("$test.in", "clear8(0) clear8(1)");
capture_ok("bfpp $test.in", <<END);
[-]>[-]<
END

# clear8 - execute a test program
spew("$test.in", "++++>++++< clear8(0) clear8(1) >1");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0 
         ^^^ (ptr=1)

END

unlink_testfiles;
done_testing;
