#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# set - error no arguments
spew("$test.in", "set");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'set'
END

# set - error empty arguments
spew("$test.in", "set()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: macro 'set' expects 2 arguments
END

# set - error too many arguments
spew("$test.in", "set(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# set - set the given cells
spew("$test.in", "set(0,3) set(1,2)");
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

# set - execute a test program
spew("$test.in", "++++ set(0,10)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape: 10 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
