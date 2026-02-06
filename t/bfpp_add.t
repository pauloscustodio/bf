#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# add - error no arguments
spew("$test.in", "add");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'add'
END

# add - error empty arguments
spew("$test.in", "add()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: macro 'add' expects 2 arguments
END

# add - error too many arguments
spew("$test.in", "add(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# add(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
add(A,B)
>A
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
>
[
  -
]
>
[
  -
]
<
[
  -
]
<
[
  ->+>+<<
]
>>
[
  -<<+>>
]
[
  -
]
<
[
  -<<+>>
]
[
  -
]
<<
END

# run add(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		add(A, B)
		>A
END
		my $R = $A + $B;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B   0   0 
     ^^^ (ptr=0)

END
	}
}

unlink_testfiles;
done_testing;
