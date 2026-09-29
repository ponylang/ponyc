## Fix malformed documentation for constructors with generic-typed default values

Generated documentation for constructors whose parameters had default values with type arguments — such as `Array[OptionSpec]()` — included the rest of the source file in the code block instead of just the constructor signature. The `CommandSpec.parent` and `CommandSpec.leaf` constructors in the `cli` package were the most visible example.
