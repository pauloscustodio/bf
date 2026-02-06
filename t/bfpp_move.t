#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# move - error no arguments
spew("$test.in", "move");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'move'
END

# move - error empty arguments
spew("$test.in", "move()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: macro 'move' expects 2 arguments
END

# move - error too many arguments
spew("$test.in", "move(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# move - move contents between cells
spew("$test.in", "move(0,1) move(1,0)");
capture_ok("bfpp $test.in", <<END);
>
[
  -
]
<
[
  ->+<
]
[
  -
]
>
[
  -<+>
]
<
END

# move - execute a test program
spew("$test.in", "++++ move(0,1)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   4 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
