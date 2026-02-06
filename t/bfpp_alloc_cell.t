#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# alloc_cell - error no arguments
spew("$test.in", "alloc_cell");
capture_nok("bfpp $test.in", <<END);
$test.in:1:11: error: expected '(' after macro name 'alloc_cell'
END

# alloc_cell - error empty arguments
spew("$test.in", "alloc_cell()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: alloc_cell expects one identifier
END

# alloc_cell - error too many arguments
spew("$test.in", "alloc_cell(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: expected ')' at end of macro call, found ','
END

# alloc_cell - wrong type of arguments
spew("$test.in", "alloc_cell(1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: alloc_cell expects one identifier
END

# alloc_cell - use as reserved word
spew("$test.in", "#define alloc_cell 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'alloc_cell': reserved word
END

# alloc_cell
spew("$test.in", <<END);
alloc_cell(A) /* A=0 */
alloc_cell(B) /* B=1 */
>A
>B
>A
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
<
END

unlink_testfiles;
done_testing;
