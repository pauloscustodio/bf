#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# repeat - error no arguments
spew("$test.in", "repeat");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected '(' after macro name 'repeat'
END

# repeat - error empty arguments
spew("$test.in", "repeat()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: macro 'repeat' expects 1 argument
END

# repeat - error too many arguments
spew("$test.in", "repeat(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# repeat - error no endrepeat
spew("$test.in", "repeat(0)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: repeat without matching endrepeat
(repeat):1:6: error: unmatched '[' instruction
(repeat):1:1: error: unmatched '{' brace
(repeat):1:8: error: unmatched '{' brace
END

# naked endrepeat
spew("$test.in", "endrepeat");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: endrepeat without matching repeat
END

# run repeat ... endrepeat
spew("$test.in", <<END);
alloc_cell8(X)
alloc_cell8(COUNT)
set8(COUNT, 3)
repeat(COUNT)
	>X +
endrepeat
>COUNT
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]+++[<+>-]
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  3   0 
         ^^^ (ptr=1)

END

unlink_testfiles;
done_testing;
