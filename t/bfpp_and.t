#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# and - error no arguments
spew("$test.in", "and");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'and'
END

# and - error empty arguments
spew("$test.in", "and()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: macro 'and' expects 2 arguments
END

# and - error too many arguments
spew("$test.in", "and(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# and(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
and(A,B)
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
>
[
  -
]
<<
[
  -
]
<<
[
  ->>+<<
]
>>>>>
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
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
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
<<<
[
  ->>>+<<<
]
+>>>>+<
[
  ->
  [
    -<<<<->>>>
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
<<
[
  -
]
<<
[
  ->>+>>+<<<<
]
>>>>
[
  -<<<<+>>>>
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
<<<<
[
  ->>
  [
    -
  ]
  <
  [
    ->+<
  ]
  <
]
<<
[
  -
]
>>>>
[
  -<<<<+>>>>
]
<<
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
<<<<
END

# run and(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		and(A, B)
		>A
END
		my $R = ($A && $B) ? 1 : 0;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B   0   0   0   0   0 
     ^^^ (ptr=0)

END
	}
}

unlink_testfiles;
done_testing;
