## Fix lambda type inference failure with generic #share constraints

A generic class with a `#share`-constrained type parameter that passed a lambda with inferred parameter types to a generic method like `Iter.map` would fail to compile with "the type parameter has no lower bounds." Explicitly annotating the lambda's parameter types worked around the issue. Inferred parameter types now compile correctly.
