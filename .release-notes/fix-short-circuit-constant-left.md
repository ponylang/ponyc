## Fix short-circuit `and`/`or` dropping side effects when left operand is a compile-time constant

When the left operand of a short-circuit `and` or `or` folded to a compile-time constant, ponyc dropped the code that computed it. Any side effects in that left operand — assignments, field writes — never ran, and the program silently produced wrong results.

```pony
var x: U32 = 1
let r = ((x = x + 1; true) and (x > 2))
// x was 1 (the assignment never ran); now x is 2
```

A related shape — a nested short-circuit that folds to a constant, used as the left operand of an outer `and` or `or` — crashed the compiler instead of producing a wrong answer.
