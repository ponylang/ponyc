## Fix false positive in `_final` send checking for generic classes

The compiler incorrectly rejected `_final` methods that called methods on generic classes instantiated with concrete type arguments. For example, calling a method on a `Generic[Prim]` where `Generic` has a trait-constrained type parameter would produce a spurious "_final cannot create actors or send messages" error, even though the concrete type is known and does not send messages.

The finaliser pass now resolves generic type parameters to their concrete types before analyzing method bodies for potential message sends. This expands the range of generic code accepted in `_final` methods, though some cases involving methods inherited through a provides chain (like `Range`) still produce false positives.
