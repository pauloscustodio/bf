#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# set8 - error no arguments
spew("$test.in", "set8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'set8'
END

# set8 - error empty arguments
spew("$test.in", "set8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: macro 'set8' expects 2 arguments
END

# set8 - error too many arguments
spew("$test.in", "set8(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# set8 - set the given cells
spew("$test.in", "set8(0,3) set8(1,2)");
capture_ok("bfpp $test.in", <<END);
[
  -
]
+++>
[
  -
]
++<
END

# set8 - execute a test program
spew("$test.in", "++++ set8(0,10)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape: 10 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
