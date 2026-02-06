#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# sub - error no arguments
spew("$test.in", "sub");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'sub'
END

# sub - error empty arguments
spew("$test.in", "sub()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: macro 'sub' expects 2 arguments
END

# sub - error too many arguments
spew("$test.in", "sub(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# sub(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
sub(A,B)
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
  -<<->>
]
[
  -
]
<<
END

# run sub(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		sub(A, B)
		>B
END
		my $R = sprintf("%3d", ($A - $B) & 0xFF);
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:$R   $B 
         ^^^ (ptr=1)

END
	}
}

unlink_testfiles;
done_testing;
