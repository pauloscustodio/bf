#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# move8 - error no arguments
spew("$test.in", "move8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected '(' after macro name 'move8'
END

# move8 - error empty arguments
spew("$test.in", "move8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: macro 'move8' expects 2 arguments
END

# move8 - error too many arguments
spew("$test.in", "move8(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected ')' at end of macro call, found ','
END

# move8 - move contents between cells
spew("$test.in", "++++ move8(0,1)");
capture_ok("bfpp $test.in", <<END);
++++>[-]<[->+<]
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   4 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
