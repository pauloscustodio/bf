#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# question mark shows usage
capture_nok("bf -?", <<'END');
usage: bf [-t] [-D] [input_file]
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
PC=0 instr=Increment(1)
Tape:  1 
     ^^^ (ptr=0)

PC=1 instr=Move(1)
Tape:  1   0 
         ^^^ (ptr=1)

PC=2 instr=Increment(2)
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

# test multiply
spew("$test.bf", "++++ [ - > +++ < ]");
capture_ok("bf -t $test.bf", <<'END');
PC=0 instr=Increment(4)
Tape:  4 
     ^^^ (ptr=0)

PC=1 instr=Multiply([1:3])
Tape:  0  12 
     ^^^ (ptr=0)

END

# test COPY(A,B,T)
spew("$test.bf", ">+++<[-]>>[-]<[->+<<+>]<[->+<][-]>");
capture_ok("bf -t $test.bf", <<'END');
PC=0 instr=Move(1)
Tape:  0   0 
         ^^^ (ptr=1)

PC=1 instr=Increment(3)
Tape:  0   3 
         ^^^ (ptr=1)

PC=2 instr=Move(-1)
Tape:  0   3 
     ^^^ (ptr=0)

PC=3 instr=Clear()
Tape:  0   3 
     ^^^ (ptr=0)

PC=4 instr=Move(2)
Tape:  0   3   0 
             ^^^ (ptr=2)

PC=5 instr=Clear()
Tape:  0   3   0 
             ^^^ (ptr=2)

PC=6 instr=Move(-1)
Tape:  0   3 
         ^^^ (ptr=1)

PC=7 instr=Multiply([1:1][-1:1])
Tape:  3   0   3 
         ^^^ (ptr=1)

PC=8 instr=Move(-1)
Tape:  3   0   3 
     ^^^ (ptr=0)

PC=9 instr=Multiply([1:1])
Tape:  0   3   3 
     ^^^ (ptr=0)

PC=10 instr=Clear()
Tape:  0   3   3 
     ^^^ (ptr=0)

PC=11 instr=Move(1)
Tape:  0   3   3 
         ^^^ (ptr=1)

END

# test scan
spew("$test.bf", "+ > + > + > + > + > + [<]");
capture_nok("bf $test.bf", <<'END');
Error: Tape pointer underflow
END

spew("$test.bf", ">>> + > + > + > + > + > + [<]");
capture_ok("bf -t $test.bf", <<'END');
PC=0 instr=Move(3)
Tape:  0   0   0   0 
                 ^^^ (ptr=3)

PC=1 instr=Increment(1)
Tape:  0   0   0   1 
                 ^^^ (ptr=3)

PC=2 instr=Move(1)
Tape:  0   0   0   1   0 
                     ^^^ (ptr=4)

PC=3 instr=Increment(1)
Tape:  0   0   0   1   1 
                     ^^^ (ptr=4)

PC=4 instr=Move(1)
Tape:  0   0   0   1   1   0 
                         ^^^ (ptr=5)

PC=5 instr=Increment(1)
Tape:  0   0   0   1   1   1 
                         ^^^ (ptr=5)

PC=6 instr=Move(1)
Tape:  0   0   0   1   1   1   0 
                             ^^^ (ptr=6)

PC=7 instr=Increment(1)
Tape:  0   0   0   1   1   1   1 
                             ^^^ (ptr=6)

PC=8 instr=Move(1)
Tape:  0   0   0   1   1   1   1   0 
                                 ^^^ (ptr=7)

PC=9 instr=Increment(1)
Tape:  0   0   0   1   1   1   1   1 
                                 ^^^ (ptr=7)

PC=10 instr=Move(1)
Tape:  0   0   0   1   1   1   1   1   0 
                                     ^^^ (ptr=8)

PC=11 instr=Increment(1)
Tape:  0   0   0   1   1   1   1   1   1 
                                     ^^^ (ptr=8)

PC=12 instr=Scan(-1)
Tape:  0   0   0   1   1   1   1   1   1 
             ^^^ (ptr=2)

END

spew("$test.bf", "+ > + > + > + > + > + <<<<< [>]");
capture_ok("bf -t $test.bf", <<'END');
PC=0 instr=Increment(1)
Tape:  1 
     ^^^ (ptr=0)

PC=1 instr=Move(1)
Tape:  1   0 
         ^^^ (ptr=1)

PC=2 instr=Increment(1)
Tape:  1   1 
         ^^^ (ptr=1)

PC=3 instr=Move(1)
Tape:  1   1   0 
             ^^^ (ptr=2)

PC=4 instr=Increment(1)
Tape:  1   1   1 
             ^^^ (ptr=2)

PC=5 instr=Move(1)
Tape:  1   1   1   0 
                 ^^^ (ptr=3)

PC=6 instr=Increment(1)
Tape:  1   1   1   1 
                 ^^^ (ptr=3)

PC=7 instr=Move(1)
Tape:  1   1   1   1   0 
                     ^^^ (ptr=4)

PC=8 instr=Increment(1)
Tape:  1   1   1   1   1 
                     ^^^ (ptr=4)

PC=9 instr=Move(1)
Tape:  1   1   1   1   1   0 
                         ^^^ (ptr=5)

PC=10 instr=Increment(1)
Tape:  1   1   1   1   1   1 
                         ^^^ (ptr=5)

PC=11 instr=Move(-5)
Tape:  1   1   1   1   1   1 
     ^^^ (ptr=0)

PC=12 instr=Scan(1)
Tape:  1   1   1   1   1   1   0 
                             ^^^ (ptr=6)

END

unlink_testfiles;
done_testing;
