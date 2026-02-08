#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# not8 - error no arguments
spew("$test.in", "not8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'not8'
END

# not8 - error empty arguments
spew("$test.in", "not8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: macro 'not8' expects 1 argument
END

# not8 - error too many arguments
spew("$test.in", "not8(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected ')' at end of macro call, found ','
END

# not8 - negate the cell value
spew("$test.in", "alloc_cell8(X) not8(X)");
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
END

# not8 - execute a test program
spew("$test.in", "alloc_cell8(X) clear8(X) not8(X)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1 
     ^^^ (ptr=0)

END

for my $i (1, 2, 254, 255) {
	spew("$test.in", "alloc_cell8(X) set8(X, $i) not8(X)");
	capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0 
     ^^^ (ptr=0)

END
}

unlink_testfiles;
done_testing;
