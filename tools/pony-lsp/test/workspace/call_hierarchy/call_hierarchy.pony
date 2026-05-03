"""
Test fixtures for exercising LSP call hierarchy functionality.

This package provides fixture files for manual and automated testing of the
three LSP call hierarchy operations (prepareCallHierarchy,
callHierarchy/incomingCalls, and callHierarchy/outgoingCalls).

Fixture layout:
- _TCallA declares f_a() which is called by f_b, f_c, f_d, and f_lambda.
- _TCallB declares f_b() which calls f_a() and is called by f_c.
- _TCallC declares f_c() which calls both f_a() and f_b().
- _TCallD declares f_d() which calls f_a() twice from separate call sites.
- _TCallActor (actor) declares new create() and be act(), where act calls f_a.
- _TCallLambda declares f_lambda() which calls f_a() twice — once before an
  embedded object literal and once after, exercising the nested-method guard.
- _TPolyA and _TPolyB both declare poly_method(); _TPolyUser calls each via
  a typed receiver, exercising position-based disambiguation for same-named
  methods in the same file.
- _TCrossCaller declares cross_call() which calls TCallee.callee_method() from
  the callee/ sub-package, exercising cross-package outgoing-call resolution.
  callee/ uses a public type (no underscore) so it is accessible cross-package.
- _TConstructed declares new create(). _TCallConstructorCaller.f_new_explicit()
  calls _TConstructed.create() explicitly, exercising that _is_synthetic_newref
  does not filter user-written Foo.create() constructor calls.
- _TConstructorWithBody declares new create(a: _TCallA) that calls a.f_a(),
  exercising outgoing call collection when the source method is a `new`.

To manually test call hierarchy functionality:
1. Open the lsp/test/workspace directory as a project in your editor.
2. Open one of the fixture files while the Pony language server is active.
3. Place your cursor on a method name and invoke Show Call Hierarchy
   (in VS Code: right-click → Show Call Hierarchy, or Shift+Alt+H).

Expected call hierarchy behaviour:

  `f_a` in `_TCallA`:
    - Prepare: one item for `f_a` with detail `_TCallA`
    - Incoming calls: `f_b` (once), `f_c` (once), `f_d` (twice, grouped),
      `f_lambda` (twice, grouped), `act` (once)
    - Outgoing calls: none

  `f_b` in `_TCallB`:
    - Prepare: one item for `f_b` with detail `_TCallB`
    - Incoming calls: `f_c` (one call site)
    - Outgoing calls: `f_a` (one call site)

  `f_c` in `_TCallC`:
    - Prepare: one item for `f_c` with detail `_TCallC`
    - Incoming calls: none
    - Outgoing calls: `f_a` (one call site) and `f_b` (one call site)

  `f_d` in `_TCallD`:
    - Prepare: one item for `f_d` with detail `_TCallD`
    - Incoming calls: none
    - Outgoing calls: `f_a` (two call sites — grouped into one entry)

  `create` in `_TCallActor`:
    - Prepare: one item for `create` with kind=constructor (9)

  `act` in `_TCallActor`:
    - Prepare: one item for `act` with kind=method (6)
    - Outgoing calls: `f_a` (one call site)

  `f_lambda` in `_TCallLambda`:
    - Prepare: one item for `f_lambda` with detail `_TCallLambda`
    - Incoming calls: none
    - Outgoing calls: `f_a` (two call sites — call inside nested object is
      excluded; only the two outer calls appear)

  `poly_method` in `_TPolyA` (cursor on line 2):
    - Incoming calls: `call_a` in `_TPolyUser` (one call site)

  `poly_method` in `_TPolyB` (cursor on line 5):
    - Incoming calls: `call_b` in `_TPolyUser` (one call site)

  `cross_call` in `_TCrossCaller`:
    - Outgoing calls: `callee_method` in `TCallee` (callee/ sub-package)

  `f_new_explicit` in `_TCallConstructorCaller`:
    - Outgoing calls: `create` in `_TConstructed` (one call site) — verifies
      that _is_synthetic_newref does not filter user-written Foo.create() calls

  `create` in `_TConstructorWithBody`:
    - Prepare: one item for `create` with kind=constructor (9)
    - Outgoing calls: `f_a` (one call site) — verifies _OutgoingCallCollector
      works when the source method is a `new`, not just `fun` or `be`
"""
