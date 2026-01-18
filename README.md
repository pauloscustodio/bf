# bf
Brainfuck

## Brainfuck Preprocessor (bfpp) Syntax

1. Top-Level Structure

program ::= { statement }

2. Statements

statement ::= bf-code
               | macro-definition
               | macro-call
               | include-directive
               | conditional

3. Raw Brainfuck Code

bf-code ::= { bf-char }

bf-char ::= "+" | "-" | "<" | ">" | "[" | "]" | "." | ","

4. Macro Definitions

macro-definition ::= "#define" identifier macro-params macro-body

macro-params ::= "(" param-list ")" | e

param-list ::= identifier { "," identifier }

macro-body ::= "{" { statement } "}"

5. Macro Calls

macro-call ::= identifier "(" arg-list ")"

arg-list ::= expression { "," expression }

6. Include Directive

include-directive ::= "#include" string-literal

string-literal ::= '"' { any-char-except-quote } '"'

7. Conditionals

conditional ::= "#if" expression block optional-else "#endif"

optional-else ::= "#else" block | e

block ::= { statement }

8. Expressions

expression ::= term { ("+" | "-") term }

term ::= number
          | identifier
          | "(" expression ")"

9. Lexical Elements

Identifiers

identifier ::= ( letter | "\_" ) { letter | digit | "\_" }

Numbers

number ::= digit { digit }

Whitespace

whitespace ::= " " | "\t" | "\n" | "\r" | "\f" | "\v"

Comments

'//' { any-char-except-newline }

'/*' { any-char-except-'*/' } '*/'

