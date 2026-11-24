## Fix JSONPath stack overflow on deeply nested `@`-relative filter chains

A JSONPath query with deeply nested `@`-relative filters (`$[?@[?@[?...`) overflowed the native stack during evaluation. Each level of `@`-relative filter nesting re-enters the evaluator through native calls, so a document with matching structure at every level consumed one native stack frame per level.

The evaluator now counts re-entry depth and returns an empty result when it exceeds 1024, the same limit the parser enforces on expression nesting.
