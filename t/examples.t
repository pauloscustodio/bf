#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# BF hello
capture_ok("bf examples/hello.bf", <<END);
Hello, World!
END

# BFPP hello
capture_ok("bfpp examples/hello.bfpp | bf", <<END);
Hello, World!
END

# simple calculator
run_ok("bfpp examples/calc.bfpp > examples/calc.bf");

spew("$test.in", " 1 + 2 + 3 + -1 ");
capture_ok("bf examples/calc.bf < $test.in", "5 \n");

spew("$test.in", " 1 * 2 * 3 * -1 ");
capture_ok("bf examples/calc.bf < $test.in", "-6 \n");

spew("$test.in", " 4 + 7 - 3 ");
capture_ok("bf examples/calc.bf < $test.in", "8 \n");

spew("$test.in", " 4 / 2 - 1 ");
capture_ok("bf examples/calc.bf < $test.in", "1 \n");

unlink_testfiles;
done_testing;
