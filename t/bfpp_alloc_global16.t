#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# alloc_global16 - error no arguments
spew("$test.in", "alloc_global16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: expected '(' after macro name 'alloc_global16'
END

# alloc_global16 - error empty arguments
spew("$test.in", "alloc_global16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:17: error: macro 'alloc_global16' expects 1 argument
END

# alloc_global16 - error too many arguments
spew("$test.in", "alloc_global16(A,B)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:17: error: expected ')' at end of macro call, found ','
END

# alloc_global16 - undefined argument
spew("$test.in", "alloc_global16(A)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:16: error: macro 'A' is not defined
END

# alloc_global16 - double call
spew("$test.in", "alloc_global16(4) alloc_global16(4)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:19: error: alloc_global16 already called
END

# alloc_global16 - access global without reserved area
spew("$test.in", "set8(global(0), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: global() called before alloc_global16
END

# alloc_global16 - access global below bounds
spew("$test.in", "alloc_global16(2) set8(global(-1), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:24: error: global(-1) overflow
END

# alloc_global16 - access global above bounds
spew("$test.in", "alloc_global16(2) set8(global(2), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:24: error: global(2) overflow
END

# alloc_global16 - use as reserved word
spew("$test.in", "#define alloc_global16 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'alloc_global16': reserved word
END

# global - use as reserved word
spew("$test.in", "#define global 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'global': reserved word
END

# double call with free
spew("$test.in", <<END);
alloc_global16(4) free_global16 alloc_global16(2)
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
alloc_global16(4) 
set8(global(0), 1)
set8(global(1), 2)
set8(global(2), 3)
set8(global(3), 4)
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
