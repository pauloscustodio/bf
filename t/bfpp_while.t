#!/usr/bin/env perl

BEGIN { use lib 't'; require 'testlib.pl'; }

use Modern::Perl;

# while - error no arguments
spew("$test.in", "while");
capture_nok("bfpp $test.in", <<END);
$test.in:1:6: error: expected '(' after macro name 'while'
END

# while - error empty arguments
spew("$test.in", "while()");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: macro 'while' expects 1 argument
END

# while - error too many arguments
spew("$test.in", "while(A,B");
capture_nok("bfpp $test.in", <<END);
$test.in:1:8: error: expected ')' at end of macro call, found ','
END

# while - error no endwhile
spew("$test.in", "while(0)");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: while without matching endwhile
(while):1:63: error: unmatched '[' instruction
(while):1:1: error: unmatched '{' brace
(while):1:65: error: unmatched '{' brace
END

# naked endwhile
spew("$test.in", "endwhile");
capture_nok("bfpp $test.in", <<END);
$test.in:1:9: error: endwhile without matching while
END

# run while ... endwhile
spew("$test.in", <<END);
alloc_cell(X)
alloc_cell(COND)
set(COND, 3)
while(COND)
	>X +
	>COND -
endwhile
>X
END
capture_ok("bfpp $test.in", <<END);
[
  -
]
>
[
  -
]
[
  -
]
+++>
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
<
[
  ->+>+<<
]
>>
[
  -<<+>>
]
[
  -
]
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
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
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
>
[
  -
]
<
[
  -
]
<
[
  ->+<
]
+>>+<
[
  ->
  [
    -<<->>
  ]
  <
]
[
  -
]
>
[
  -
]
<<
[
  <<+>->>
  [
    -
  ]
  <
  [
    -
  ]
  <
  [
    ->+>+<<
  ]
  >>
  [
    -<<+>>
  ]
  [
    -
  ]
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
  <
  [
    ->+<
  ]
  +>>+<
  [
    ->
    [
      -<<->>
    ]
    <
  ]
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
  >
  [
    -
  ]
  <
  [
    -
  ]
  <
  [
    ->+<
  ]
  +>>+<
  [
    ->
    [
      -<<->>
    ]
    <
  ]
  [
    -
  ]
  >
  [
    -
  ]
  <<
]
[
  -
]
<<
END
capture_ok("bfpp $test.in | bf -D", <<END);
Tape:  3   0   0   0   0 
     ^^^ (ptr=0)

END

unlink_testfiles;
done_testing;
