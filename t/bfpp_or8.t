#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# or8 - error no arguments
spew("$test.in", "or8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'or8'
END

# or8 - error empty arguments
spew("$test.in", "or8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: macro 'or8' expects 2 arguments
END

# or8 - error too many arguments
spew("$test.in", "or8(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# or8(a,b)
spew("$test.in", <<END);
alloc_cell8(A)
alloc_cell8(B)
or8(A,B)
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

# run or8(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell8(A)
		alloc_cell8(B)
		set8(A, $A)
		set8(B, $B)
		or8(A, B)
		>B
END
		my $R = ($A || $B) ? 1 : 0;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B 
         ^^^ (ptr=1)

END
	}
}

unlink_testfiles;
done_testing;
