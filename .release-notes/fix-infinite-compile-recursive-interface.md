## Fix compiler hang and crash on recursive generic types

Previously, the compiler would hang or crash when type-checking several shapes of recursive generic type. All three cases below now compile or produce a normal type error instead of looping or segfaulting.

A recursive generic interface whose method return type references the same interface with strictly larger type arguments would hang the compiler indefinitely. Two drift shapes were affected:

```pony
// Drifting via tuple typeargs (ponylang/ponyc#1216).
interface Iter[A]
  fun enum[B](): Iter[(B, A)] => this
```

```pony
// Drifting via nested wrapping (ponylang/ponyc#5198).
interface I[A]
  fun f(): I[I[A]] => this
```

A type parameter whose constraint references the parameter itself would crash the compiler with a stack overflow:

```pony
// Recursive type parameter constraint (ponylang/ponyc#3930).
fun flatten[A: Array[Array[A]] #read](arrayin: Array[Array[A]]): Array[A] =>
  ...
```

The root cause of the crash on recursive type parameter constraints, and of the residual exponential work that made the recursive interface hangs unreachable to the divergence guard, was that `exact_nominal` in the structural subtype checker compared typeargs via `is_eq_typeargs`, which calls back into the subtype machinery and re-enters `check_assume` on the same recursive shapes. Replacing that with a structural string comparison (originally authored by Red Davies for ponylang/ponyc#3930) eliminates the re-entry. Combined with the new `SAME_DEF_LIMIT` divergence guard in `is_nominal_sub_nominal`, which bounds the depth of any single drifting recursion chain, the compiler now terminates on all three shapes above.
