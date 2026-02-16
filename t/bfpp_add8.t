#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# add8 - error no arguments
spew("$test.in", "add8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'add8'
END

# add8 - error empty arguments
spew("$test.in", "add8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: macro 'add8' expects 2 arguments
END

# add8 - error too many arguments
spew("$test.in", "add8(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# add8(a,b)
spew("$test.in", <<END);
alloc_cell8(A)
alloc_cell8(B)
add8(A,B)
>A
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]>[-]>[-]<[-]<[->+>+<<]>>[-<<+>>][-]<[-<<+>>][-]<<
END

# run add8(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell8(A)
		alloc_cell8(B)
		set8(A, $A)
		set8(B, $B)
		add8(A, B)
		>B
END
		my $R = $A + $B;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B 
         ^^^ (ptr=1)

END
	}
}

unlink_testfiles;
done_testing;
