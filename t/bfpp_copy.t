#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# copy - error no arguments
spew("$test.in", "copy");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected '(' after macro name 'copy'
END

# copy - error empty arguments
spew("$test.in", "copy()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: macro 'copy' expects 2 arguments
END

# copy - error too many arguments
spew("$test.in", "copy(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: expected ')' at end of macro call, found ','
END

# copy - copy contents between cells
spew("$test.in", "set(1,4) copy(1,2) copy(1,3)");
capture_ok("bfpp $test.in", <<END);
>
[
  -
]
++++<
[
  -
]
>>
[
  -
]
<
[
  ->+<<+>
]
<
[
  ->+<
]
[
  -
]
[
  -
]
>>>
[
  -
]
<<
[
  ->>+<<<+>
]
<
[
  ->+<
]
[
  -
]
END

# copy - execute a test program
# Note: cell 0 is the temp used by copy
spew("$test.in", ">++++ copy(1,2)");
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   4   4 
         ^^^ (ptr=1)

END

unlink_testfiles;
done_testing;
