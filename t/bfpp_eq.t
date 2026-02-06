#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# eq - error no arguments
spew("$test.in", "eq");
capture_nok("bfpp $test.in", <<END);
$test.in:1:3: error: expected '(' after macro name 'eq'
END

# eq - error empty arguments
spew("$test.in", "eq()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: macro 'eq' expects 2 arguments
END

# eq - error too many arguments
spew("$test.in", "eq(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected ')' at end of macro call, found ','
END

# eq(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
eq(A,B)
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
<<
[
  ->>+<<
]
+>>>+<
[
  ->
  [
    -<<<->>>
  ]
  <
]
[
  -
]
>
[
  -
]
<<<
END

# run eq(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		eq(A, B)
		>B
END
		my $R = ($A == $B) ? 1 : 0;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B 
         ^^^ (ptr=1)

END
	}
}

unlink_testfiles;
done_testing;
