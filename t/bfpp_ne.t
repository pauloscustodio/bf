#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# ne - error no arguments
spew("$test.in", "ne");
capture_nok("bfpp $test.in", <<END);
$test.in:1:3: error: expected '(' after macro name 'ne'
END

# ne - error empty arguments
spew("$test.in", "ne()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: macro 'ne' expects 2 arguments
END

# ne - error too many arguments
spew("$test.in", "ne(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected ')' at end of macro call, found ','
END

# ne(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
ne(A,B)
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
<
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

# run ne(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		ne(A, B)
		>B
END
		my $R = ($A != $B) ? 1 : 0;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B 
         ^^^ (ptr=1)

END
	}
}

unlink_testfiles;
done_testing;
