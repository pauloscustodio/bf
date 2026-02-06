#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# #if error - no expression
spew("$test.in", <<END);
#if 
END
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: missing expression after #if
END

# #if error - tokens after expression
spew("$test.in", <<END);
#if 1 !
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:1:7: error: unexpected token after #if: '!'
END

# #if error - no #endif
spew("$test.in", <<END);
#if 1
END
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: unterminated #if (missing #endif)
END

# #if error - two #else - if true
spew("$test.in", <<END);
#if 1
#else
#else 
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:3:7: error: multiple #else in the same #if
END

# #if error - two #else - if false
spew("$test.in", <<END);
#if 0
#else
#else 
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:3:7: error: multiple #else in the same #if
END

# #if error - #elsif after #else - if true
spew("$test.in", <<END);
#if 1
#else
#elsif 1
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:3:8: error: #elsif after #else
END

# #if error - #elsif after #else - if false
spew("$test.in", <<END);
#if 0
#else
#elsif 1
#endif
END
capture_nok("bfpp $test.in", <<END);
$test.in:3:8: error: #elsif after #else
END

# #if - simple
spew("$test.in", <<END);
#if 1
+
#endif
END
capture_ok("bfpp $test.in", <<END);

+
END

# #if - simple
spew("$test.in", <<END);
#if 0
+
#endif
END
capture_ok("bfpp $test.in", <<END);
END

# #if with variable - false
spew("$test.in", <<END);
#if X
+
#endif
END
capture_ok("bfpp $test.in", <<END);
END

# #if with variable - true
spew("$test.in", <<END);
#if X
+
#endif
END
capture_ok("bfpp -D X=1 $test.in", <<END);

+
END

# #if with #else - false
spew("$test.in", <<END);
#if 0
+
#else
-
#endif
END
capture_ok("bfpp $test.in", <<END);



-
END

# #if with #else - true
spew("$test.in", <<END);
#if 1
+
#else
-
#endif
END
capture_ok("bfpp $test.in", <<END);

+
END

# #if - #elsif - #elsif - #else - #endif - take branch 1
spew("$test.in", <<END);
#if 1
+
#elsif 1
++
#elsif 1
+++
#else
++++
#endif
END
capture_ok("bfpp $test.in", <<END);

+
END

# #if - #elsif - #elsif - #else - #endif - take branch 2
spew("$test.in", <<END);
#if 0
+
#elsif 1
++
#elsif 1
+++
#else
++++
#endif
END
capture_ok("bfpp $test.in", <<END);



++
END

# #if - #elsif - #elsif - #else - #endif - take branch 3
spew("$test.in", <<END);
#if 0
+
#elsif 0
++
#elsif 1
+++
#else
++++
#endif
END
capture_ok("bfpp $test.in", <<END);





+++
END

# #if - #elsif - #elsif - #else - #endif - take branch 4
spew("$test.in", <<END);
#if 0
+
#elsif 0
++
#elsif 0
+++
#else
++++
#endif
END
capture_ok("bfpp $test.in", <<END);







++++
END

unlink_testfiles;
done_testing;
