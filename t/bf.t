#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# question mark shows usage
capture_nok("bf -?", <<'END');
Error: Usage: bf [-t] [-D] [input_file]
END

# move past the beginning of the tape issues an error
spew("$test.bf", "<");
capture_nok("bf < $test.bf", <<'END');
Error: Tape pointer underflow
END

# unclosed loop
spew("$test.bf", "[");
capture_nok("bf < $test.bf", <<'END');
Error: Unmatched '['
END

# unclosed loop
spew("$test.bf", "]");
capture_nok("bf < $test.bf", <<'END');
Error: Unmatched ']'
END

# dump result
spew("$test.bf", "+ > ++ > +++ < <");
capture_ok("bf -D < $test.bf", <<'END');
Tape:  1   2   3 
     ^^^ (ptr=0)

END

spew("$test.bf", "+ > ++ > +++ <");
capture_ok("bf -D < $test.bf", <<'END');
Tape:  1   2   3 
         ^^^ (ptr=1)

END

# read file
spew("$test.bf", "+ > ++ > +++ <");
capture_ok("bf -D $test.bf", <<'END');
Tape:  1   2   3 
         ^^^ (ptr=1)

END

# trace
spew("$test.bf", "+ > ++");
capture_ok("bf -t $test.bf", <<'END');
PC=0 instr='+'
Tape:  1 
     ^^^ (ptr=0)

PC=1 instr='>'
Tape:  1   0 
         ^^^ (ptr=1)

PC=2 instr='+'
Tape:  1   1 
         ^^^ (ptr=1)

PC=3 instr='+'
Tape:  1   2 
         ^^^ (ptr=1)

END

# test loops
spew("$test.bf", "+++ [ - ]");
capture_ok("bf -D $test.bf", <<'END');
Tape:  0 
     ^^^ (ptr=0)

END

# test output
my $prog = "";
for (split //, "Hello\n") {
	$prog .= ("+" x ord($_)).". [-] ";
}
spew("$test.bf", $prog);
capture_ok("bf $test.bf", <<'END');
Hello
END

# test input
spew("$test.in", "!");
spew("$test.bf", ",");
capture_ok("bf -D $test.bf < $test.in", <<'END');
Tape: 33 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
