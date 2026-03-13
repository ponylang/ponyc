## Fix slow compilation of programs using recursive union types

Programs using recursive union types — where a type alias like `type Expr is (A | B | C)` has members whose fields are themselves `Expr` — could experience significantly slower compilation due to redundant subtype checking work.

The compiler's subtype checker uses an assumption stack to break cycles during recursive type checks, but this mechanism only operated on nominal types. When a recursive union type appeared as a field in its own members, the compiler would re-expand the full union membership at every level of recursion, leading to exponential work in the expression pass.

The assumption stack now also covers union types, preventing re-expansion of the same union subtype check during recursive field type checking. In our benchmarks, this cut compilation time roughly in half for programs with deeply recursive union types, with no measurable impact on programs that don't use them.
