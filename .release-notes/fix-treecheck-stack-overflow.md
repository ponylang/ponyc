## Fix stack overflow in AST tree checker

The compiler's `--checktree` AST validation pass used deep mutual recursion that could overflow the default 1MB Windows stack when validating programs with deeply nested expression trees. The tree checker has been converted to use an iterative worklist instead.

This is unlikely to affect end users. The `--checktree` flag is a compiler development tool used primarily when building ponyc itself and running the test suite.
