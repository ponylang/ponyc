## Add missing NULL checks for gen_expr results in gencall.c

Two additional code paths in the compiler could crash instead of reporting errors when a receiver sub-expression encountered a codegen error. These are the same class of bug as the recently fixed crash when calling methods on invalid shift expressions, but in the `gen_funptr` and `gen_pattern_eq` code paths. These are harder to trigger in practice but could cause segfaults in the LLVM optimizer if encountered.
