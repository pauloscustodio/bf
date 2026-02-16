#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# neg8 - error no arguments
spew("$test.in", "neg8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'neg8'
END

# neg8 - error empty arguments
spew("$test.in", "neg8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: macro 'neg8' expects 1 argument
END

# neg8 - error too many arguments
spew("$test.in", "neg8(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected ')' at end of macro call, found ','
END

# neg8 - negate the cell value
spew("$test.in", "alloc_cell8(X) neg8(X)");
capture_ok("bfpp $test.in", <<END);
[-]>[-]>[-]>[-]<[-]<<[->>+>+<<<]>>>[-<<<+>>>][-]<[-<->][-]<<[-]>[-<+>][-]<
END

# neg8 - execute a test program
for my $A (0, 1, 2, 127, 128, 255) {
	spew("$test.in", "alloc_cell8(X) set8(X, $A) neg8(X)");
	my $R = sprintf("%3d", (-$A) & 0xFF);
	capture_ok("bfpp $test.in | bf -D", <<END);
Tape:$R 
     ^^^ (ptr=0)

END
}

unlink_testfiles;
done_testing;
