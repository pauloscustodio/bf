#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# set16 - error no arguments
spew("$test.in", "set16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected '(' after macro name 'set16'
END

# set16 - error empty arguments
spew("$test.in", "set16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: macro 'set16' expects 2 arguments
END

# set16 - error too many arguments
spew("$test.in", "set16(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected ')' at end of macro call, found ','
END

# set16 - set16 the given cells
spew("$test.in", "set16(0,256+2) set16(2,511) >3");
capture_ok("bfpp $test.in", <<END);
[
  -
]
++>
[
  -
]
+>
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
+
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  2   1 255   1 
                 ^^^ (ptr=3)

END

unlink_testfiles;
done_testing;
