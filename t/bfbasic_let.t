#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# LET v = number
spew("$test.bas", <<END);
LET A = 10
END
run_ok("bfbasic -o $test.bfpp $test.bas");
check_text_file("$test.bfpp", <<END);
alloc_cell16(A)
set16(A, 10)
END
capture_ok("bfpp $test.bfpp | bf -D", <<END);
Tape: 10 
     ^^^ (ptr=0)

END

# LET v = var
spew("$test.bas", <<END);
LET A = 10
LET B = A
END
run_ok("bfbasic -o $test.bfpp $test.bas");
check_text_file("$test.bfpp", <<END);
alloc_cell16(A)
alloc_cell16(B)
set16(A, 10)
copy16(A, B)
END
capture_ok("bfpp $test.bfpp | bf -D", <<END);
Tape: 10   0  10 
     ^^^ (ptr=0)

END

# v = number
spew("$test.bas", <<END);
A = 10
END
run_ok("bfbasic -o $test.bfpp $test.bas");
check_text_file("$test.bfpp", <<END);
alloc_cell16(A)
set16(A, 10)
END
capture_ok("bfpp $test.bfpp | bf -D", <<END);
Tape: 10 
     ^^^ (ptr=0)

END

# v = var
spew("$test.bas", <<END);
A = 10
B = A
END
run_ok("bfbasic -o $test.bfpp $test.bas");
check_text_file("$test.bfpp", <<END);
alloc_cell16(A)
alloc_cell16(B)
set16(A, 10)
copy16(A, B)
END
capture_ok("bfpp $test.bfpp | bf -D", <<END);
Tape: 10   0  10 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
