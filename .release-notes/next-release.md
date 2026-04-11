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

## Fix pony-lsp hanging on startup on Windows

pony-lsp was unresponsive on Windows when launched by an editor. The LSP base protocol uses explicit `\r\n` sequences in message headers, but Windows opens stdout in text mode by default, which translates every `\n` to `\r\n`. This turned the header separator `\r\n\r\n` into `\r\r\n\r\r\n` on the wire — a sequence that LSP clients don't recognize, causing them to wait forever for the end of the headers.

pony-lsp now sets stdout to binary mode on Windows at startup, so `\r\n` is written to the pipe unchanged.

## Fix type checking failure for interfaces with interdependent type parameters

Previously, interfaces with multiple type parameters where one parameter appeared as a type argument to the same interface would fail to type check:

```pony
interface State[S, I, O]
  fun val apply(state: S, input: I): (S, O)
  fun val bind[O2](next: State[S, O, O2]): State[S, I, O2]
```

```
Error:
type argument is outside its constraint
  argument: O #any
  constraint: O2 #any
```

The compiler replaced type variables one at a time during reification, so replacing `S` with its value could inadvertently transform a different parameter's constraint before that parameter was processed. This has been fixed by replacing all type variables in a single pass.

## Fix incorrect code generation for `this->` in lambda type parameters

When a lambda type used `this->` for viewpoint adaptation (e.g., `{(this->A)}`), the compiler desugared it into an anonymous interface where `this` incorrectly referred to the interface's own receiver rather than the enclosing class's receiver. This caused wrong vtable dispatch, incorrect results, or segfaults when the lambda was forwarded to another function.

```pony
class Container[A: Any #read]
  fun box apply(f: {(this->A)}) =>
    f(_value)
```

The desugaring now correctly preserves the polymorphic behavior of `this->` across different receiver capabilities.

## Add LSP go-to-definition for type aliases

The Pony language server now supports go-to-definition on type alias names. For example, placing the cursor on `Map` in `Map[String, U32]` and invoking go-to-definition navigates to the `type Map` declaration in the standard library. Previously, go-to-definition only worked on the type arguments (`String`, `U32`) but not on the alias name itself.

This also works for local type aliases defined in the same package.

## Fix soundness hole in match capture bindings

Match `let` bindings with viewpoint-adapted or generic types could bypass the compiler's capability checks, allowing creation of multiple `iso` references to the same object. A direct `let x: Foo iso` capture was correctly rejected, but `let x: this->B iso` and `let x: this->T` (where T could be iso) slipped through because viewpoint adaptation through `box` erases the ephemeral marker that the existing check relies on to detect unsoundness.

The compiler now checks whether a capture type has a capability that would change under aliasing (iso, trn, or a generic cap that includes them) and rejects the capture when the match expression isn't ephemeral. Previously-accepted code that hits this check was unsound and could segfault at runtime.

### How to fix code broken by this change

Consume the match expression so the discriminee is ephemeral:

Before (unsound, now rejected):

```pony
match u
| let ut: T =>
  do_something(consume ut)
else
  (consume u, default())
end
```

After:

```pony
match consume u
| let ut: T =>
  do_something(consume ut)
| let uu: U =>
  (consume uu, default())
end
```

The `else` branch becomes `| let uu: U =>` because `u` is consumed and no longer available. For field access, use a destructive read (`match field_name = sentinel_value`) to get an ephemeral result.

## Fix segfault when matching tuple elements against unions or interfaces via Any

Matching a tuple element against a union or interface type when the tuple was accessed through `Any val` caused a segfault at runtime:

```pony
actor Main
  new create(env: Env) =>
    let x: Any val = ("a", U8(1))
    match x
    | (let a: String, let b: (U8 | U16)) =>
      env.out.print(b.string())  // segfault
    end
```

The same crash happened when matching against an interface like `Stringable` instead of a union. Matching against concrete types (e.g., `let b: U8`) was unaffected.

This has been fixed. Tuple elements that are unboxed primitives are now correctly boxed when the match pattern requires a pointer representation.

## Add `DocumentHighlightKind` to pony-lsp document highlights

The `textDocument/documentHighlight` response now includes a `kind` field on each highlight. Editors that support `DocumentHighlightKind` (such as VS Code) use it to color read and write occurrences of a symbol differently, making it easier to spot where a value is consumed versus where it is assigned.

The three kinds from the LSP spec are applied as follows:

- **Write**: the occurrence is a value-binding declaration (`var f: T`, `let x: T = expr`, a parameter, or a field), or the target of an assignment (`x = expr`). All declaration sites are Write, consistent with mainstream LSP convention (clangd, rust-analyzer, tsserver).
- **Read**: the occurrence is a reference — reading a field or local variable, calling a function, behavior, or constructor.
- **Text**: the occurrence is a method, function, type, or other declaration where read/write semantics do not apply, or a type reference.

The `kind` field was always optional in the spec and defaulted to Text, so editors that do not support per-kind highlighting are unaffected.

## Add generic type parameter highlights to pony-lsp

Placing the cursor on a generic type parameter (e.g., `T` in `class Foo[T]` or `fun apply[T](...)`) now highlights all occurrences of that parameter across the method or class body. Both the declaration site and every use site are returned as `Text` (kind 1) highlights.

## Add `textDocument/references` for generic type parameters in pony-lsp

Placing the cursor on a generic type parameter declaration (e.g., `T` in `class Foo[T]` or `actor Bar[T: Any val]`) and invoking Find All References now returns all occurrences correctly. Two fixes were required:

- **Zero results from declaration site**: the cursor-side `tk_id` node was not promoted to its parent `tk_typeparam` before resolution, so the walker never matched any references.
- **Phantom results in generic actors**: ponyc synthesizes a nominal return type for constructors and behaviours in generic types, producing an internal `tk_new`/`tk_be` → `tk_nominal` → `tk_typeargs` → `tk_typeparamref` chain. This phantom node resolved to the type parameter and appeared as a spurious reference; it is now filtered in both `textDocument/references` and `textDocument/documentHighlight`.

