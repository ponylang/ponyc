## Add style/docstring-leading-blank lint rule

pony-lint now flags docstrings where a blank line immediately follows the opening `"""`. The first line of content should begin on the line right after the opening delimiter.

```pony
// Flagged — blank line after opening """
class Foo
  """

  Foo docstring.
  """

// Clean — content starts on the next line
class Foo
  """
  Foo docstring.
  """
```

Types and methods annotated with `\nodoc\` are exempt, consistent with `style/docstring-format`.

## Fix rare silent connection hangs on macOS and BSD

On macOS and BSD, if the OS failed to fully register an I/O event (due to resource exhaustion or an FD race), the failure was silently ignored. Actors waiting for network events that were never registered would hang indefinitely with no error. This could appear as connections that never complete, listeners that stop accepting, or timers that stop firing — with no indication of what went wrong.

The runtime now detects these registration failures and notifies the affected actor, which tears down cleanly — the same as any other I/O failure. Stdlib consumers like `TCPConnection` and `TCPListener` handle this automatically.

If you implement `AsioEventNotify` outside the stdlib, you can now detect registration failures with the new `AsioEvent.errored` predicate. Without handling it, a failure is silently ignored (the same behavior as before, but now you have the option to detect it):

```pony
be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
  if AsioEvent.errored(flags) then
    // Registration failed — tear down
    _close()
    return
  end
  // ... normal event handling
```

## Fix rare silent connection hangs on Linux

On Linux, if the OS failed to register an I/O event (due to resource exhaustion or an FD race), the failure was silently ignored. Actors waiting for network events that were never registered would hang indefinitely with no error. This could appear as connections that never complete, listeners that stop accepting, or timers that stop firing — with no indication of what went wrong.

The runtime now detects these registration failures and notifies the affected actor, which tears down cleanly — the same as any other I/O failure. Stdlib consumers like `TCPConnection` and `TCPListener` handle this automatically.

Also fixes the ASIO backend init to correctly detect `epoll_create1` and `eventfd` failures (previously checked for 0 instead of -1), and to clean up all resources on partial init failure.

## LSP: Fix goto_definition range end

The Pony language server `textDocument/definition` response now returns a correct `range.end` position. Previously, it had an off-by-one error in the column value.

## Add hierarchical configuration for pony-lint

pony-lint now supports `.pony-lint.json` files in subdirectories, not just at the project root. A subdirectory config overrides the root config for all files in that subtree, using the same JSON format.

For example, to turn off the `style/package-docstring` rule for everything under your `examples/` directory, add an `examples/.pony-lint.json`:

```json
{"rules": {"style/package-docstring": "off"}}
```

Precedence follows proximity — the nearest directory with a setting wins. Category entries (e.g., `"style": "off"`) override parent rule-specific entries in that category. Omitting a rule from a subdirectory config defers to the parent, not the default.

Malformed subdirectory configs produce a `lint/config-error` diagnostic and fall through to the parent config — the subtree is still linted, just with the parent's rules.

## Protect pony-lint against oversize configuration files

pony-lint now rejects `.pony-lint.json` files larger than 64 KB. With hierarchical configuration, each directory in a project can have its own config file — an unexpectedly large file could cause excessive memory consumption. Config files that exceed the limit produce a `lint/config-error` diagnostic with the file size and path.

## Protect pony-lint against oversize ignore files

pony-lint now rejects `.gitignore` and `.ignore` files larger than 64 KB. With hierarchical ignore loading, each directory in a project can have its own ignore files — an unexpectedly large file could cause excessive memory consumption. Ignore files that exceed the limit or that cannot be opened produce a `lint/ignore-error` diagnostic with exit code 2.

## Add LSP textDocument/documentHighlight support

The Pony language server now handles `textDocument/documentHighlight` requests. Placing the cursor on any symbol highlights all occurrences in the file, covering fields, locals, parameters, constructors, functions, behaviours, and type names.

## Fix pony-lsp failures with some code constructs

Fixed go-to-definition failing for type arguments inside generic type aliases. For example, go-to-definition on `String` or `U32` in `Map[String, U32]` now correctly navigates to their definitions. Previously, these positions returned no result.

## Fix silent timer hangs on Linux

On Linux, if a timer system call failed (due to resource exhaustion or other system error), the failure was silently ignored. Actors waiting for timer notifications would hang indefinitely with no error — timers that should fire simply never did.

The runtime now detects timer setup and arming failures and notifies the affected actor, which tears down cleanly — the same as any other I/O failure. Stdlib consumers like `Timers` handle this automatically.

## Add LSP `textDocument/inlayHint` support

`pony-lsp` now supports inlay hints. Editors that request `textDocument/inlayHint` will receive inline type annotations after the variable name for `let` and `var` declarations whose type is inferred rather than explicitly written.

## Add LSP `textDocument/references` support

The Pony language server now handles `textDocument/references` requests. References searches across all packages in the workspace, and supports the `includeDeclaration` option to optionally include the definition site in the results.

## Fix pony-lsp hanging after shutdown and exit

pony-lsp would hang indefinitely after receiving the LSP `shutdown` request followed by the `exit` notification. The process had to be killed manually. The exit handler now properly disposes all actors, allowing the runtime to shut down cleanly.

