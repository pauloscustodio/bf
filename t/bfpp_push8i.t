#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# push8i - error no arguments
spew("$test.in", "push8i");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: expected '(' after macro name 'push8i'
END

# push8i - error empty arguments
spew("$test.in", "push8i()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: macro 'push8i' expects 1 argument
END

# push8i - error too many arguments
spew("$test.in", "push8i(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# push8i - copies immediate to stack, decrements stack pointer
spew("$test.in", "push8i(127) push8i(63)");
capture_ok("bfpp $test.in", <<END);
>>[-]+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++++++++++++++<<[-]+++++++++++++++++++++++
++++++++++++++++++++++++++++++++++++++++
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape: 63   0 127 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
