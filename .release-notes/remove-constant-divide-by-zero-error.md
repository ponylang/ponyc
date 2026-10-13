## Remove compile-time error for constant integer division by zero

The compiler no longer rejects constant integer division or remainder by zero. `let x: I64 = 1 / 0` was a compile error, but `let x = I64(1) / I64(0)` was not — the check only caught bare literals.
