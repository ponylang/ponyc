## Fix pony-lsp inability to find the standard library

Previously, pony-lsp was unable to locate the Pony standard library on its own. It relied entirely on the `PONYPATH` environment variable to find packages like `builtin`. This meant that while the VS Code extension could work around the issue by configuring the path explicitly, other editors using pony-lsp would fail with errors like "couldn't locate this path in file builtin".

pony-lsp now automatically discovers the standard library by finding its own executable directory and searching for packages relative to it — the same approach that ponyc uses. Since pony-lsp is installed alongside ponyc, the standard library is found without any manual configuration, making pony-lsp work out of the box with any editor.

## Fix persistent HashMap returning incorrect results for None values

The persistent `HashMap` used `None` as an internal sentinel to signal "key not found" in its lookup methods. This collided with user value types that include `None` (e.g., `Map[String, (String | None)]`). Using `HashMap` with a `None` value could lead to errors in user code as "it was none" and "it wasn't present" were impossible to distinguish.

The internal lookup methods now use `error` instead of `None` to signal a missing key, so all value types work correctly.

This is a breaking change for any code that was depending on the previous (incorrect) behavior. For example, code that expected `apply` to raise for keys mapped to `None`, or that relied on `contains` returning `false` for `None`-valued entries, will now see correct results instead.

