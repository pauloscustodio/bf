#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# alloc_cell16 - error no arguments
spew("$test.in", "alloc_cell16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: expected '(' after macro name 'alloc_cell16'
END

# alloc_cell16 - error empty arguments
spew("$test.in", "alloc_cell16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: macro 'alloc_cell16' expects one identifier
END

# alloc_cell16 - error too many arguments
spew("$test.in", "alloc_cell16(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: expected ')' at end of macro call, found ','
END

# alloc_cell16 - wrong type of arguments
spew("$test.in", "alloc_cell16(1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:16: error: macro 'alloc_cell16' expects one identifier
END

# alloc_cell16 - use as reserved word
spew("$test.in", "#define alloc_cell16 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'alloc_cell16': reserved word
END

# alloc_cell16
spew("$test.in", <<END);
alloc_cell16(A) /* A=0 */
alloc_cell16(B) /* B=2 */
>A + > ++
>B +++ > ++++
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]>[-]>[-]<<<+>++>+++>++++
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1   2   3   4 
                 ^^^ (ptr=3)

END

unlink_testfiles;
done_testing;
