#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# or - error no arguments
spew("$test.in", "or");
capture_nok("bfpp $test.in", <<END);
$test.in:1:3: error: expected '(' after macro name 'or'
END

# or - error empty arguments
spew("$test.in", "or()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: macro 'or' expects 2 arguments
END

# or - error too many arguments
spew("$test.in", "or(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected ')' at end of macro call, found ','
END

# or(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
or(A,B)
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
  ->>+<<
]
>
[
  ->+<
]
>>
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
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
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
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
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
<<<<<<
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

# run or(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		or(A, B)
		>A
END
		my $R = ($A || $B) ? 1 : 0;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B   0   0   0   0   0 
     ^^^ (ptr=0)

END
	}
}

unlink_testfiles;
done_testing;
