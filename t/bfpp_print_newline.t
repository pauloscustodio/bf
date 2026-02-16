#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# print_newline - print the given string
spew("$test.in", <<END);
print_newline
print_newline
print_newline
END
capture_ok("bfpp $test.in", <<END);
[-]++++++++++.[-]++++++++++.[-]++++++++++.[-]
END
capture_ok("bfpp $test.in | bf", <<END);



END

unlink_testfiles;
done_testing;
