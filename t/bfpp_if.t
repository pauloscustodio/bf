#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# if - error no arguments
spew("$test.in", "if");
capture_nok("bfpp $test.in", <<END);
$test.in:1:3: error: expected '(' after macro name 'if'
END

# if - error empty arguments
spew("$test.in", "if()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: macro 'if' expects 1 argument
END

# if - error too many arguments
spew("$test.in", "if(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: expected ')' at end of macro call, found ','
END

# if - error no endif
spew("$test.in", "if(0)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: if without matching endif
(if):1:177: error: unmatched '[' instruction
(if):1:1: error: unmatched '{' brace
(if):1:179: error: unmatched '{' brace
END

# naked else
spew("$test.in", "else");
capture_nok("bfpp $test.in", <<END);
$test.in:1:5: error: else without matching if
END

# naked endif
spew("$test.in", "endif");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: endif without matching if
END

# if - endif
spew("$test.in", <<END);
alloc_cell8(X)
alloc_cell8(Y)
set8(X, 0)
if(X)
	>Y +
endif
>Y
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]<[-]>>[-]>[-]>[-]<[-]<<<[->>>+>+<<<<]>>>>[-<<<<+>>>>][-]>[-]<[-]<[->+<]+>
>+<[->[-<<->>]<][-]>[-]<[-]<<[-]>[-<+>>+<]>[-<+>][-]>[-]<[-]<<[->>+<<]+>>>+<[->[
-<<<->>>]<][-]>[-]<<<[<+>-][-]>[-]<<
END

# run if - endif
spew("$test.in", <<END);
alloc_cell8(X)
alloc_cell8(Y)
set8(X, 0)
if(X)
	>Y +
endif
>Y
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0 
         ^^^ (ptr=1)

END

spew("$test.in", <<END);
alloc_cell8(X)
alloc_cell8(Y)
set8(X, 123)
if(X)
	>Y +
endif
>Y
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:123   1 
         ^^^ (ptr=1)

END

# if - else - endif
spew("$test.in", <<END);
alloc_cell8(X)
alloc_cell8(Y)
alloc_cell8(Z)
set8(X, 0)
if(X)
	>Y +
else
	>Z +
endif
>Y
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]>[-]<<[-]>>>[-]>[-]>[-]<[-]<<<<[->>>>+>+<<<<<]>>>>>[-<<<<<+>>>>>][-]>[-]<
[-]<[->+<]+>>+<[->[-<<->>]<][-]>[-]<[-]<<[-]>[-<+>>+<]>[-<+>][-]>[-]<[-]<<[->>+<
<]+>>>+<[->[-<<<->>>]<][-]>[-]<<<[<<+>>-]>[<<+>>-]<[-]>[-]<<<
END

# run if - else - endif
spew("$test.in", <<END);
alloc_cell8(X)
alloc_cell8(Y)
alloc_cell8(Z)
set8(X, 0)
if(X)
	>Y +
else
	>Z +
endif
>Z
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  0   0   1 
             ^^^ (ptr=2)

END

spew("$test.in", <<END);
alloc_cell8(X)
alloc_cell8(Y)
alloc_cell8(Z)
set8(X, 1)
if(X)
	>Y +
else
	>Z +
endif
>Z
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  1   1   0 
             ^^^ (ptr=2)

END

unlink_testfiles;
done_testing;
