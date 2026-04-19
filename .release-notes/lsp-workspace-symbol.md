## Add LSP `workspace/symbol` support

The Pony language server now handles `workspace/symbol` requests, enabling workspace-wide symbol search (e.g. "Go to Symbol in Workspace" / Cmd+T in VS Code).

Given a query string, the server performs a case-insensitive substring search over all compiled symbols — top-level types (class, actor, struct, primitive, trait, interface) and their members (constructors, functions, behaviours, fields) — across every package in the workspace. An empty query returns all symbols. Results are returned as a flat `SymbolInformation[]` with file URI and source range; member symbols include a `containerName` identifying the enclosing type.

The server advertises `workspaceSymbolProvider: true` in its capabilities.
