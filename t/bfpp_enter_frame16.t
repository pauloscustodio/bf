#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# enter_frame16 - error no arguments
spew("$test.in", "enter_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:14: error: expected '(' after macro name 'enter_frame16'
END

# enter_frame16 - error empty arguments
spew("$test.in", "enter_frame16()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:16: error: macro 'enter_frame16' expects 2 arguments
END

# enter_frame16 - error too many arguments
spew("$test.in", "enter_frame16(A,B,C)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:18: error: expected ')' at end of macro call, found ','
END

# enter_frame16 - undefined argument
spew("$test.in", "enter_frame16(A,0) leave_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:15: error: macro 'A' is not defined
END

# leave_frame16 without enter_frame16
spew("$test.in", "leave_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: unmatched leave_frame16 instruction
END

# unclosed frame at the end
spew("$test.in", "enter_frame16(0, 0)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: unmatched enter_frame16 instruction
END

# enter_frame16 - access arg without reserved area
spew("$test.in", "set8(arg(0), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: arg() instruction outside alloc_frame16
END

# enter_frame16 - access local without reserved area
spew("$test.in", "set8(local(0), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: local() instruction outside alloc_frame16
END

# enter_frame16 - access frame_temp without reserved area
spew("$test.in", "set8(frame_temp(0), 1)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: frame_temp() instruction outside alloc_frame16
END

# enter_frame16 - no arguments on stack
spew("$test.in", "enter_frame16(1, 0)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:1: error: enter_frame16: not enough arguments on stack (expected 1 x16-bit)
END

# enter_frame16 - access arg below bounds
spew("$test.in", "enter_frame16(0, 1) set8(arg(-1), 1) leave_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:26: error: arg(-1) overflow
END

# enter_frame16 - access arg above bounds
spew("$test.in", "enter_frame16(0, 1) set8(arg(1), 1) leave_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:26: error: arg(1) overflow
END

# enter_frame16 - access local below bounds
spew("$test.in", "enter_frame16(0, 1) set8(local(-1), 1) leave_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:26: error: local(-1) overflow
END

# enter_frame16 - access local above bounds
spew("$test.in", "enter_frame16(0, 1) set8(local(1), 1) leave_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:26: error: local(1) overflow
END

# enter_frame16 - access frame_temp below bounds
spew("$test.in", "enter_frame16(0, 1) set8(frame_temp(-1), 1) leave_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:26: error: frame_temp(-1) overflow
END

# enter_frame16 - access frame_temp above bounds
spew("$test.in", "enter_frame16(0, 1) frame_alloc_temp16(1) set8(frame_temp(1), 1) leave_frame16");
capture_nok("bfpp $test.in", <<END);
$test.in:1:48: error: frame_temp(1) overflow
END

# enter_frame16 - use as reserved word
spew("$test.in", "#define enter_frame16 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'enter_frame16': reserved word
END

# leave_frame16 - use as reserved word
spew("$test.in", "#define leave_frame16 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'leave_frame16': reserved word
END

# frame_alloc_temp16 - use as reserved word
spew("$test.in", "#define frame_alloc_temp16 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'frame_alloc_temp16': reserved word
END

# arg - use as reserved word
spew("$test.in", "#define arg 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'arg': reserved word
END

# local - use as reserved word
spew("$test.in", "#define local 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'local': reserved word
END

# frame_temp - use as reserved word
spew("$test.in", "#define frame_temp 1");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: cannot define macro 'frame_temp': reserved word
END

# enter_frame16 without arguments - reserves one for return value
spew("$test.in", <<END);
alloc_cell8(A)
enter_frame16(0, 0)
	set8(arg(0), 12)
leave_frame16
pop8(A)
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]++++++++++++<[-]>[-<+>]<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape: 12 
     ^^^ (ptr=0)

END

# enter_frame16 with one argument f(a) = a-2
spew("$test.in", <<END);
alloc_cell8(A)
push8i(12)
enter_frame16(1, 0)
	{ >(arg(0)) -- }
leave_frame16
pop8(A)
END
capture_ok("bfpp $test.in", <<END);
[-]>[-]++++++++++++--<[-]>[-<+>]<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape: 10 
     ^^^ (ptr=0)

END

# enter_frame16 with two arguments f(a,b) = a+b
spew("$test.in", <<END);
alloc_cell8(A)
push8i(2)
push8i(7)
enter_frame16(2, 0)
	add8(arg(0), arg(1))
leave_frame16
pop8(A)
END
capture_ok("bfpp $test.in", <<END);
[-]>>>>>[-]++<<[-]+++++++<<[-]>[-]<[-]>>[-<<+>+>]<[->+<][-]<[->>>>+<<<<][-]<[-]>
>>>>[-<<<<<+>>>>>]<<<<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  9   0   0   7 
     ^^^ (ptr=0)

END

# enter_frame16 with one argument and two locals  - f(a)=2*a
spew("$test.in", <<END);
alloc_cell8(A)
push8i(2)
enter_frame16(1, 2)
	copy8(arg(0), local(0))
	copy8(local(0), local(1))
	add8(local(1), local(0))
	copy8(local(1), arg(0))
leave_frame16
pop8(A)
END
capture_ok("bfpp $test.in", <<END);
[-]>>>>>>>[-]++<<<<<<[-]>>>>[-]>>[-<<+<<<<+>>>>>>]<<<<<<[->>>>>>+<<<<<<][-]>>[-]
>>[-<<+<<+>>>>]<<<<[->>>>+<<<<][-]>[-]<[-]>>>>[-<<<<+>+>>>]<<<[->>>+<<<][-]<[->>
+<<][-]>>>>>>[-]<<<<[->>>>+<<<<<<+>>]<<[->>+<<][-]<[-]>>>>>>>[-<<<<<<<+>>>>>>>]<
<<<<<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  4   0   0   4   0   2 
     ^^^ (ptr=0)

END

# enter_frame16 with one argument and two temps  - f(a)=2*a
spew("$test.in", <<END);
alloc_cell8(A)
push8i(2)
enter_frame16(1, 0)
frame_alloc_temp16(2)
	copy8(arg(0), frame_temp(0))
	copy8(frame_temp(0), frame_temp(1))
	add8(frame_temp(1), frame_temp(0))
	copy8(frame_temp(1), arg(0))
leave_frame16
pop8(A)
END
capture_ok("bfpp $test.in", <<END);
[-]>>>>>>>[-]++<<<<<<[-]>>>>[-]>>[-<<+<<<<+>>>>>>]<<<<<<[->>>>>>+<<<<<<][-]>>[-]
>>[-<<+<<+>>>>]<<<<[->>>>+<<<<][-]>[-]<[-]>>>>[-<<<<+>+>>>]<<<[->>>+<<<][-]<[->>
+<<][-]>>>>>>[-]<<<<[->>>>+<<<<<<+>>]<<[->>+<<][-]<[-]>>>>>>>[-<<<<<<<+>>>>>>>]<
<<<<<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  4   0   0   4   0   2 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
