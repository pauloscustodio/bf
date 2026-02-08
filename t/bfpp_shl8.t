#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# shl8 - error no arguments
spew("$test.in", "shl8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'shl8'
END

# shl8 - error empty arguments
spew("$test.in", "shl8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: macro 'shl8' expects 2 arguments
END

# shl8 - error too many arguments
spew("$test.in", "shl8(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# shl8(a,b)
spew("$test.in", <<END);
alloc_cell8(A)
alloc_cell8(B)
shl8(A,B)
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
<<
[
  ->>+>+<<<
]
>>>
[
  -<<<+>>>
]
[
  -
]
<
[
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
    ->>+>+<<<
  ]
  >>>
  [
    -<<<+>>>
  ]
  [
    -
  ]
  <
  [
    -<<<<+>>>>
  ]
  [
    -
  ]
  <-
]
<
[
  -
]
>
[
  -
]
<<<
END

# run shl8(a,b)
for my $A (0, 1, 2, 4, 8, 16, 32, 64, 128, 255) {
	for my $B (1, 2, 4) {
		spew("$test.in", <<END);
		alloc_cell8(A)
		alloc_cell8(B)
		set8(A, $A)
		set8(B, $B)
		shl8(A, B)
		>B
END
		my $R = sprintf("%3d", ($A << $B) & 0xFF);
		capture_ok("bfpp $test.in | bf -D", <<END);
Tape:$R   $B 
         ^^^ (ptr=1)

END
	}
}

unlink_testfiles;
done_testing;
