## Compile-time string literal concatenation

The compiler now folds adjacent string literal concatenation at compile time, avoiding runtime allocation and copying. This works across chains that mix literals and variables — adjacent literals are merged while non-literal operands are left as runtime `.add()` calls. For example, `"a" + "b" + x + "c" + "d"` is folded to the equivalent of `"ab".add(x).add("cd")`, reducing four runtime concatenations down to two.
