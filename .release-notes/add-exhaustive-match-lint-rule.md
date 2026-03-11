## Add safety/exhaustive-match lint rule to pony-lint

pony-lint now flags exhaustive `match` expressions that don't use the `\exhaustive\` annotation. Without `\exhaustive\`, adding a new variant to a union type compiles silently — the compiler injects `else None` for missing cases instead of raising an error. The new `safety/exhaustive-match` rule catches these matches so you can add the annotation and get compile-time protection against incomplete case handling.

The rule is enabled by default. To suppress it for a specific match, use `// pony-lint: allow safety/exhaustive-match` on the line before the match, or disable the entire `safety` category with `--disable safety` or in your config file.

This is the first rule in the new `safety` category. It also introduces `CompileSession` to the `pony_compiler` library, enabling resumable compilation (compile to `PassParse`, inspect the AST, then continue to `PassExpr`).
