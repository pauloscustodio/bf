#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# print_char8 - error no arguments
spew("$test.in", "print_char8");
capture_nok("bfpp $test.in", <<END);
$test.in:1:12: error: expected '(' after macro name 'print_char8'
END

# print_char8 - error empty arguments
spew("$test.in", "print_char8()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: macro 'print_char8' expects 1 argument
END

# print_char8 - error too many arguments
spew("$test.in", "print_char8(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: expected ')' at end of macro call, found ','
END

# print_char8 - print the given cells
spew("$test.in", <<END);
alloc_cell8(T)
set8(T, 'H') print_char8(T)
set8(T, 'e') print_char8(T)
set8(T, 'l') print_char8(T)
set8(T, 'l') print_char8(T)
set8(T, 'o') print_char8(T)
set8(T, 10)  print_char8(T)
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+++++++++++++++++++++.
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++.
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++.
[
  -
]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+++++++++++++++++++++++++++++++.
[
  -
]
++++++++++.
END
capture_ok("bfpp $test.in | bf", <<END);
Hello
END

unlink_testfiles;
done_testing;
