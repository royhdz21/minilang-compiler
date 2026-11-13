# Mini-Lang Specification

## 1. Lexical structure

| Category    | Definition |
|-------------|------------|
| Whitespace  | space, tab, CR, LF (ignored) |
| Comments    | `// until end of line` and `/* block, not nested */` |
| Keywords    | `int bool if else while print read true false` |
| Identifier  | `[a-zA-Z_][a-zA-Z0-9_]*` (max 63 characters) |
| Integer     | `[0-9]+` (must fit in a 32-bit signed int) |
| Operators   | `+ - * / < <= > >= == != = && \|\| !` |
| Delimiters  | `; , ( ) { }` |

Lexical errors (unknown character, lone `&` or `|`, unterminated comment,
too-large literal, letters glued to a number) are reported as
`file:line:col: error: message`; the scanner skips the offending input and continues.

## 2. Grammar (EBNF)

```
program  = { stmt } ;
stmt     = type ID { "," ID } ";"
         | ID "=" expr ";"
         | "if" "(" expr ")" stmt [ "else" stmt ]
         | "while" "(" expr ")" stmt
         | "print" "(" expr ")" ";"
         | "read" "(" ID ")" ";"
         | "{" { stmt } "}" ;
type     = "int" | "bool" ;

expr     = or ;
or       = and { "||" and } ;
and      = equality { "&&" equality } ;
equality = relation { ( "==" | "!=" ) relation } ;
relation = additive { ( "<" | "<=" | ">" | ">=" ) additive } ;
additive = term { ( "+" | "-" ) term } ;
term     = unary { ( "*" | "/" ) unary } ;
unary    = ( "!" | "-" ) unary | primary ;
primary  = NUM | "true" | "false" | ID | "(" expr ")" ;
```

Precedence (high to low): unary `! -`; `* /`; `+ -`; `< <= > >=`; `== !=`; `&&`; `||`.
All binary operators are left-associative. The dangling `else` binds to the nearest `if`.

## 3. Static semantics

* Every variable must be declared before use. A declaration is visible until the end of
  its enclosing block; blocks (`{ ... }`) open a new scope and may shadow outer names.
* No two declarations of the same name in the same scope.
* Types: `int` and `bool`; there are no implicit conversions.

| Construct | Rule | Result |
|-----------|------|--------|
| `+ - * /`, unary `-` | `int` operands | `int` |
| `< <= > >=` | `int` operands | `bool` |
| `== !=` | both operands of the same type | `bool` |
| `&& \|\|`, `!` | `bool` operands | `bool` |
| `x = e;` | type of `e` equals declared type of `x` | – |
| `if (e)`, `while (e)` | `e` is `bool` | – |
| `read(x);` | `x` is `int` | – |
| `print(e);` | any type | – |

## 4. Dynamic semantics

* Integers are 32-bit two's complement with wrap-around; division truncates toward zero.
  Division by zero is a run-time error.
* Variables start at 0 / `false`.
* `&&` and `||` are short-circuit.
* `print` of an `int` writes it followed by a newline; of a `bool` writes `true` / `false`.
* `read(x)` reads the next whitespace-separated integer from the input.

## 5. Three-Address Code (TAC)

A `.tac` file is a sequence of lines. `#` starts a comment (the compiler writes the symbol
table in comments at the top). Operands are integer constants (`-?[0-9]+`) or names of
variables / temporaries. Booleans are represented as `0` / `1`. Labels are `L<n>:`
(a label may also precede an instruction on the same line).

| Instruction | Meaning |
|-------------|---------|
| `x := a` | copy |
| `x := a op b` | `op` ∈ `+ - * / < <= > >= == != && \|\|` (relational results are 0/1) |
| `x := - a` , `x := ! a` | arithmetic negation, logical not |
| `goto L` | unconditional jump |
| `if a goto L` | jump when `a` ≠ 0 |
| `ifFalse a goto L` | jump when `a` = 0 |
| `if a relop b goto L` | jump when the comparison holds (`relop` ∈ `< <= > >= == !=`) |
| `L:` | label |
| `print a` / `printb a` | print integer / print `true`,`false` |
| `read x` | read an integer into `x` |
| `halt` | stop (reaching the end also stops) |

Compiler conventions:

* Temporaries are named `t1, t2, ...`; labels `L1, L2, ...`.
* A source variable called like a temporary (`t1`) or shadowing another variable is given a
  distinct storage name (`t1_1`, `x_1`, ...). The mapping is listed in the TAC header.
* Conditions of `if`/`while` compile to conditional jumps directly (no boolean temporary)
  when possible, e.g. `if (x < y) A else B` becomes
  `if x < y goto L1 / B / goto L2 / L1: / A / L2:`.
