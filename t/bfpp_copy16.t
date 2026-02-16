#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# copy16 - error no arguments
spew("$test.in", "copy16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected '(' after macro name 'copy16'
END

# copy16 - error empty arguments
spew("$test.in", "copy16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: macro 'copy16' expects 2 arguments
END

# copy16 - error too many arguments
spew("$test.in", "copy16(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:11: error: expected ')' at end of macro call, found ','
END

# copy16 - copy16 contents between cells
spew("$test.in", "set16(2, 511) copy16(2, 4)");
capture_ok("bfpp $test.in", <<END);
>>[-]+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++>[-]+<<<[-]>>>>[-]<<[->>+<<<<+>>]<<[->>+<<][-]>>>>>[-]<<[->>
+<<<<<+>>>]<<<[->>>+<<<][-]
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0 255   1 255   1 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
