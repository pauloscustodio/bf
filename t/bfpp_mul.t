#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# mul - error no arguments
spew("$test.in", "mul");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'mul'
END

# mul - error empty arguments
spew("$test.in", "mul()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: macro 'mul' expects 2 arguments
END

# mul - error too many arguments
spew("$test.in", "mul(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# mul(a,b)
spew("$test.in", <<END);
alloc_cell(A)
alloc_cell(B)
mul(A,B)
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
<
[
  -
]
<<<
[
  ->>>+>+<<<<
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
<<
[
  <<<->>>>
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
    ->>>+>+<<<<
  ]
  >>>>
  [
    -<<<<+>>>>
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
  [
    -
  ]
  <
  [
    -
  ]
  <<<
  [
    ->>>+>+<<<<
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
  <<
]
[
  -
]
<<<
[
  -
]
>>
[
  -<<+>>
]
[
  -
]
<<
END

# run mul(a,b)
for my $A (0, 1, 2) {
	for my $B (0, 1, 2) {
		spew("$test.in", <<END);
		alloc_cell(A)
		alloc_cell(B)
		set(A, $A)
		set(B, $B)
		mul(A, B)
		>B
END
		my $R = $A * $B;
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  $R   $B 
         ^^^ (ptr=1)

END
	}
}

unlink_testfiles;
done_testing;
