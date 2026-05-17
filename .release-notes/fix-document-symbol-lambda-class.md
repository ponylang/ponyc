## Fix LSP document symbol outline dropping class members after lambda field initialisers

If a class contained a `let` field whose initialiser was a lambda literal (e.g. `let _f: {(U32): U32} val = {(x: U32): U32 => x}`), all class members declared after that field were silently missing from the document symbol outline (the structure view shown by editors).

The outline now correctly includes all named class members regardless of whether the class contains lambda field initialisers, lambda-typed fields, methods returning lambdas, or methods with lambda-typed parameters.
