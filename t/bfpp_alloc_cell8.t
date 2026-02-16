#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# alloc_cell8 - error no arguments
spew("$test.in", "alloc_cell8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:12: error: expected '(' after macro name 'alloc_cell8'
END

# alloc_cell8 - error empty arguments
spew("$test.in", "alloc_cell8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: macro 'alloc_cell8' expects one identifier
END

# alloc_cell8 - error too many arguments
spew("$test.in", "alloc_cell8(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: expected ')' at end of macro call, found ','
END

# alloc_cell8 - wrong type of arguments
spew("$test.in", "alloc_cell8(1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: macro 'alloc_cell8' expects one identifier
END

# alloc_cell8 - use as reserved word
spew("$test.in", "#define alloc_cell8 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'alloc_cell8': reserved word
END

# alloc_cell8
spew("$test.in", <<END);
alloc_cell8(A) /* A=0 */
alloc_cell8(B) /* B=1 */
>A +
>B ++
>B
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]<+>++
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1   2 
         ^^^ (ptr=1)

END

unlink_testfiles;
done_testing;
