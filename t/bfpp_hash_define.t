#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# #define error - no name
spew("$test.in", "#define");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected macro name
END

# #define error - reserved name
spew("$test.in", "#define if 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'if': reserved word
END

# #define error - duplicate parameters
spew("$test.in", <<END);
#define X(A,A) 	\\
	+A
END
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: duplicate parameter name 'A' in macro 'X'
END

# #define error - reserved parameter
spew("$test.in", "#define X(if)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:11: error: cannot define parameter 'if': reserved word
END

# #define error - duplicate macro
spew("$test.in", <<END);
#define X 1
#define X 2
END
capture_nok("bfpp $test.in", <<END);
$test.in:2:9: error: macro 'X' redefined
$test.in:1:9: note: previous definition was here
END

# #define - single-line object macro
spew("$test.in", <<END);
#define A B
#define B C
#define C 4
+A
END
capture_ok("bfpp $test.in", <<END);
++++
END

# #define - multi-line object macro
spew("$test.in", <<END);
#define X	\\
[-]+'H'.
X
END
capture_ok("bfpp $test.in", <<END);
[-]++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++.
END

# #define - multi-line function macro
spew("$test.in", <<END);
#define COPY(A,B,T)	\\
{ >B [-] >T [-] >A [ - >B + >T + >A ] >T [ - >A + >T ] }
++++
COPY(0,1,2)
END
capture_ok("bfpp $test.in", <<END);
++++>[-]>[-]<<[->+>+<<]>>[-<<+>>]<<
END

unlink_testfiles;
done_testing;
