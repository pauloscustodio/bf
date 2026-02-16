#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# print_string - error no arguments
spew("$test.in", "print_string");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: expected '(' after macro name 'print_string'
END

# print_string - error empty arguments
spew("$test.in", "print_string()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: macro 'print_string' expects one string
END

# print_string - error too many arguments
spew("$test.in", "print_string(\"A\", \"B\")");
capture_nok("bfpp $test.in", <<END);
$test.in:1:17: error: expected ')' at end of macro call, found ','
END

# print_string - wrong type of argument
spew("$test.in", "print_string(42)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:17: error: macro 'print_string' expects one string
END

# print_string - print the given string
spew("$test.in", <<END);
print_string("Hello") print_newline
END
capture_ok("bfpp $test.in", <<END);
[-]++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.[-]+
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++.[-]++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++.[-]++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++.[-]++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+++++++++++++++++++++++++++++++++++++++.[-]++++++++++.[-]
END
capture_ok("bfpp $test.in | bf", <<END);
Hello
END

unlink_testfiles;
done_testing;
