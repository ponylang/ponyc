## Fix pony-lsp inability to find the standard library

Previously, pony-lsp was unable to locate the Pony standard library on its own. It relied entirely on the `PONYPATH` environment variable to find packages like `builtin`. This meant that while the VS Code extension could work around the issue by configuring the path explicitly, other editors using pony-lsp would fail with errors like "couldn't locate this path in file builtin".

pony-lsp now automatically discovers the standard library by finding its own executable directory and searching for packages relative to it — the same approach that ponyc uses. Since pony-lsp is installed alongside ponyc, the standard library is found without any manual configuration, making pony-lsp work out of the box with any editor.
