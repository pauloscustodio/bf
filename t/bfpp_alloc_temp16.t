#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# alloc_temp16 - error no arguments
spew("$test.in", "alloc_temp16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:13: error: expected '(' after macro name 'alloc_temp16'
END

# alloc_temp16 - error empty arguments
spew("$test.in", "alloc_temp16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: macro 'alloc_temp16' expects 1 argument
END

# alloc_temp16 - error too many arguments
spew("$test.in", "alloc_temp16(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: expected ')' at end of macro call, found ','
END

# alloc_temp16 - undefined argument
spew("$test.in", "alloc_temp16(A)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: macro 'A' is not defined
END

# alloc_temp16 - double call
spew("$test.in", "alloc_temp16(4) alloc_temp16(4)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:17: error: alloc_temp16 already called
END

# alloc_temp16 - access global without reserved area
spew("$test.in", "set8(temp(0), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: temp() called before alloc_temp16
END

# alloc_temp16 - access global below bounds
spew("$test.in", "alloc_temp16(2) set8(temp(-1), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:22: error: temp(-1) overflow
END

# alloc_temp16 - access global above bounds
spew("$test.in", "alloc_temp16(2) set8(temp(2), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:22: error: temp(2) overflow
END

# alloc_temp16 - use as reserved word
spew("$test.in", "#define alloc_temp16 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'alloc_temp16': reserved word
END

# temp - use as reserved word
spew("$test.in", "#define temp 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'temp': reserved word
END

# double call with free
spew("$test.in", <<END);
alloc_temp16(4) free_temp16 alloc_temp16(2)
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
>
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
>
[
  -
]
>
[
  -
]
<<<<<<<
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
>
[
  -
]
<<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0 
     ^^^ (ptr=0)

END

# alloc globals and use their addresses
spew("$test.in", <<END);
alloc_temp16(4) 
set8(temp(0), 1)
set8(temp(1), 2)
set8(temp(2), 3)
set8(temp(3), 4)
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
>
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
>
[
  -
]
>
[
  -
]
<<<<<<<
[
  -
]
+>>
[
  -
]
++>>
[
  -
]
+++>>
[
  -
]
++++<<<<<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1   0   2   0   3   0   4 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
