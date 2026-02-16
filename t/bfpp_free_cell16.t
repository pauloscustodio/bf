#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# free_cell16 - error no arguments
spew("$test.in", "free_cell16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:12: error: expected '(' after macro name 'free_cell16'
END

# free_cell16 - error empty arguments
spew("$test.in", "free_cell16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: macro 'free_cell16' expects one identifier
END

# free_cell16 - error too many arguments
spew("$test.in", "free_cell16(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: expected ')' at end of macro call, found ','
END

# free_cell16 - wrong type of arguments
spew("$test.in", "free_cell16(1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: macro 'free_cell16' expects one identifier
END

# free_cell16 - use as reserved word
spew("$test.in", "#define free_cell16 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'free_cell16': reserved word
END

# free_cell16 - use after free
spew("$test.in", "alloc_cell16(A) free_cell16(A) >A");
capture_nok("bfpp $test.in", <<END);
$test.in:1:33: error: macro 'A' is not defined
END

# free_cell16
spew("$test.in", <<END);
alloc_cell16(A) /* A=0 */ >A + > ++
alloc_cell16(B) /* B=2 */ >B +++ > ++++
alloc_cell16(C) /* C=4 */ >C +++++ > ++++++
free_cell16(B)
alloc_cell16(X) /* X=2 */ >X +++++++ > ++++++++
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]<+>++>[-]>[-]<+++>++++>[-]>[-]<+++++>++++++<<<[-]>[-]<[-]>[-]<+++++++>+++
+++++
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1   2   7   8   5   6 
                 ^^^ (ptr=3)

END

unlink_testfiles;
done_testing;
