## Single-subtype devirtualization for interface/trait dispatch

Calling a method through an interface or trait now uses a direct call instead of a vtable lookup when the program has exactly one concrete type implementing that interface. LLVM can then inline the direct call and optimize across the call boundary.

This matters most for code that passes closures or iterators through generic combinators like `Iter.fold` — the lambda and iterator calls that were previously indirect become direct calls, eligible for inlining.

