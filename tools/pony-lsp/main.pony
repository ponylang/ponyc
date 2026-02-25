"""
## The Pony Language Server

This package implements the (Language Server Protocol Spec
v3.17](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)
for Pony.

It is using the `pony_compiler` library to parse and type-check pony projects
opened in the editor, using the same code as the official compiler `ponyc`, so
the diagnostics and code analysis provided will always be in sync with what the
`ponyc` compiler provides.

## Package Contents

### `lsp`

This package contains all the code supporting the protocol, data structures and types from the spec, the communication
with a language client. The [`LanguageServer`](lsp-LanguageServer.md) is
controlling which requests and notifications are supported and dispatches to the
actor handling the workspace for the given request.

- [`Channel`](lsp-Channel.md) defining the necessary channel behaviours and [`Stdio`](lsp-Stdio.md) being the implementation for Standard-IO based communication.
- [`PonyCompiler`](lsp-PonyCompiler.md) as an actor whose main job is to expose
  the `pony_compiler` library functionality of parsing and typechecking a pony project and to
  linearize all requests as the underlying libponyc is not 100% thread-safe.

### `lsp/workspace`

This package contains the actual logic applied to handling a single pony
project. The [`WorkspaceManager`](lsp-workspace-WorkspaceManager.md) actor handles
requests and notifications, starts the parsing and typechecking process and
receives its results. It builds up the necessary state needed for answering LSP
client requests in the [`PackageState`](lsp-workspace-PackageState.md) and
[`DocumentState`](lsp-workspace-DocumentState.md) classes.

"""
use "path:../lib/ponylang/peg/"
use "path:../lib/ponylang/pony_compiler/"
use "path:../lib/mfelsche/pony-immutable-json/"
use "files"
use "pony_compiler"

actor Main
  new create(env: Env) =>
    let channel = Stdio(env.out, env.input)
    let pony_path =
      match PonyPath(env)
      | let p: String => p
      | None => ""
      end
    let language_server = LanguageServer(channel, env, pony_path)
    // at this point the server should listen to incoming messages via stdin
