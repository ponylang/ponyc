## Fix compiler hang and crash on recursive generic types

Previously, the compiler would hang or crash on two shapes of recursive generic type. Both now compile or produce a normal type error.

A recursive generic interface whose method return type references the same interface with strictly larger type arguments would hang the compiler indefinitely:

```pony
// Drifting via tuple typeargs (ponylang/ponyc#1216).
interface Iter[A]
  fun enum[B](): Iter[(B, A)] => this
```

A type parameter whose constraint references the parameter itself would crash the compiler with a stack overflow:

```pony
// Recursive type parameter constraint (ponylang/ponyc#3930).
fun flatten[A: Array[Array[A]] #read](arrayin: Array[Array[A]]): Array[A] =>
  ...
```

The root cause was that `exact_nominal` in the structural subtype checker compared typeargs via `is_eq_typeargs`, which calls back into the subtype machinery and re-enters `check_assume` on the same recursive shapes. Replacing that with a structural AST equality check that compares definition pointers directly — without re-entering the subtype check — eliminates the re-entry while preserving semantic identity (two type parameters that share a source name in different scopes are correctly distinguished). Red Davies originally authored a fix along these lines for ponylang/ponyc#3930. Combined with the new `SAME_DEF_LIMIT` divergence guard in `is_nominal_sub_nominal`, which bounds the depth of any single drifting recursion chain, the compiler now terminates on both shapes above.
