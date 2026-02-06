#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# clear - error no arguments
spew("$test.in", "clear");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected '(' after macro name 'clear'
END

# clear - error empty arguments
spew("$test.in", "clear()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: macro 'clear' expects 1 argument
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
spew("$test.in", "++++>++++< clear(0) clear(1) >1");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0 
         ^^^ (ptr=1)

END

unlink_testfiles;
done_testing;
