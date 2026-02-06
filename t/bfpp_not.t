#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# not - error no arguments
spew("$test.in", "not");
capture_nok("bfpp $test.in", <<END);
$test.in:1:4: error: expected '(' after macro name 'not'
END

# not - error empty arguments
spew("$test.in", "not()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: macro 'not' expects 1 argument
END

# not - error too many arguments
spew("$test.in", "not(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected ')' at end of macro call, found ','
END

# not - negate the cell value
spew("$test.in", "alloc_cell(X) not(X)");
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

# not - execute a test program
spew("$test.in", "alloc_cell(X) clear(X) not(X)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1 
     ^^^ (ptr=0)

END

for my $i (1, 2, 254, 255) {
	spew("$test.in", "alloc_cell(X) set(X, $i) not(X)");
	capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0 
     ^^^ (ptr=0)

END
}

unlink_testfiles;
done_testing;
