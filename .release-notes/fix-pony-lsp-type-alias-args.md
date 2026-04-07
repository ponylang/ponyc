## Fix pony-lsp failures with some code constructs

Fixed go-to-definition failing for type arguments inside generic type aliases. For example, go-to-definition on `String` or `U32` in `Map[String, U32]` now correctly navigates to their definitions. Previously, these positions returned no result.
