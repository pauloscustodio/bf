#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# test #include errors
spew("$test.in", "#include");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected string literal after #include
END

spew("$test.in", "#include 123");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected string literal after #include
END

spew("$test.inc", "");
spew("$test.in", "#include \"$test.inc\" 123");
capture_nok("bfpp $test.in", <<END);
$test.in:1:41: error: unexpected token after #include: '123'
END

unlink("$test.inc");
spew("$test.in", "#include \"$test.inc\"");
capture_nok("bfpp $test.in", <<END);
$test.in:1:40: error: cannot open file '$test.inc'
END

# test #include files
spew("$test.inc", "+++");
spew("$test.in", <<END);
#include "$test.inc"
>
#include "$test.inc"
>
END
capture_ok("bfpp $test.in", <<END);
+++>+++>
END

# include from -I path
path("$test.dir")->mkpath;
spew("$test.dir/$test.inc", "+++");
spew("$test.in", <<END);
#include "$test.inc"
>
END
capture_ok("bfpp -I $test.dir $test.in", <<END);
+++>
END
path("$test.dir")->remove_tree;

unlink_testfiles;
done_testing;
