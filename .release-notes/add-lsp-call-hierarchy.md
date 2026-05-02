## Add LSP call hierarchy support

The Pony language server now supports the LSP call hierarchy protocol:

- `textDocument/prepareCallHierarchy` — when the cursor is on a method or constructor, returns a `CallHierarchyItem` describing it.
- `callHierarchy/incomingCalls` — returns items for all methods in the workspace that call the given method, with the specific call sites within each caller.
- `callHierarchy/outgoingCalls` — returns items for all methods called by the given method, with the specific call sites within it.

Editors that support this protocol (VS Code, Neovim, etc.) expose it as "Show Call Hierarchy", "Show Incoming Calls", and "Show Outgoing Calls" commands.
