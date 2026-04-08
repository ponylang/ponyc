## Add LSP go-to-definition for type aliases

The Pony language server now supports go-to-definition on type alias names. For example, placing the cursor on `Map` in `Map[String, U32]` and invoking go-to-definition navigates to the `type Map` declaration in the standard library. Previously, go-to-definition only worked on the type arguments (`String`, `U32`) but not on the alias name itself.

This also works for local type aliases defined in the same package.
