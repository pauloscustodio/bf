#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# free_cell - error no arguments
spew("$test.in", "free_cell");
capture_nok("bfpp $test.in", <<END);
$test.in:1:10: error: expected '(' after macro name 'free_cell'
END

# free_cell - error empty arguments
spew("$test.in", "free_cell()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:12: error: macro 'free_cell' expects one identifier
END

# free_cell - error too many arguments
spew("$test.in", "free_cell(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:12: error: expected ')' at end of macro call, found ','
END

# free_cell - wrong type of arguments
spew("$test.in", "free_cell(1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: macro 'free_cell' expects one identifier
END

# free_cell - use as reserved word
spew("$test.in", "#define free_cell 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'free_cell': reserved word
END

# free_cell - use after free
spew("$test.in", "alloc_cell(A) free_cell(A) >A");
capture_nok("bfpp $test.in", <<END);
$test.in:1:29: error: macro 'A' is not defined
END

# free_cell
spew("$test.in", <<END);
alloc_cell(A) /* A=0 */
alloc_cell(B) /* B=1 */
alloc_cell(C) /* C=2 */
free_cell(B)
alloc_cell(X) /* X=1 */
>A
>X
>C
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
>
[
  -
]
<
[
  -
]
[
  -
]
<
END

unlink_testfiles;
done_testing;
