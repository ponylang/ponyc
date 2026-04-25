use ".."
use "pony_test"
use "files"
use "json"

primitive _SignatureHelpIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_SigHelpSimpleParam0Test.create(server))
    test(_SigHelpFullLabelTest.create(server))
    test(_SigHelpMultiParam0Test.create(server))
    test(_SigHelpMultiParam1Test.create(server))
    test(_SigHelpMultiParam2Test.create(server))
    test(_SigHelpNotInCallTest.create(server))
    test(_SigHelpNoArgsTest.create(server))
    test(_SigHelpConstructorTest.create(server))
    test(_SigHelpConstructorOpenParenTest.create(server))
    test(_SigHelpConstructorCloseParenTest.create(server))
    test(_SigHelpCalleeTest.create(server))
    test(_SigHelpDocstringTest.create(server))
    test(_SigHelpParamTextTest.create(server))
    test(_SigHelpAfterCommaTest.create(server))
    test(_SigHelpAfterCommaMultilineTest.create(server))
    test(_SigHelpAfterOpenParenMultilineTest.create(server))
    test(_SigHelpMultilineArg0Test.create(server))
    test(_SigHelpMultilineArg1Test.create(server))
    test(_SigHelpMultilineArg2Test.create(server))
    test(_SigHelpBeMethodTest.create(server))
    test(_SigHelpNestedTest.create(server))
    test(_SigHelpGenericTest.create(server))
    test(_SigHelpNamedArgTest.create(server))

class \nodoc\ iso _SigHelpSimpleParam0Test is UnitTest
  """
  Cursor on `4` in `42` (line 62, col 25) inside `_SigTarget(0).simple(42)`.
  Expects signature for `simple` with activeParameter=0.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/simple_param0"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(62, 25, _SigHelpChecker("simple", 0))])

class \nodoc\ iso _SigHelpFullLabelTest is UnitTest
  """
  Cursor on `4` in `42` (line 62, col 25) inside `_SigTarget(0).simple(42)`.
  Asserts the exact label "fun box simple(x: U32 val): U32 val", catching
  regressions in capability, parentheses, parameter list, or return type.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/full_label"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [ (62, 25,
        _SigHelpChecker(
          "simple",
          0
          where expected_full_label' = "fun box simple(x: U32 val): U32 val"))])

class \nodoc\ iso _SigHelpMultiParam0Test is UnitTest
  """
  Cursor on `1` (line 63, col 24) inside `multi(1, ...)` — first argument.
  Expects signature for `multi` with activeParameter=0.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/multi_param0"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(63, 24, _SigHelpChecker("multi", 0))])

class \nodoc\ iso _SigHelpMultiParam1Test is UnitTest
  """
  Cursor on `h` in `"hello"` (line 63, col 28) — second argument of `multi`.
  Expects signature for `multi` with activeParameter=1.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/multi_param1"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(63, 28, _SigHelpChecker("multi", 1))])

class \nodoc\ iso _SigHelpMultiParam2Test is UnitTest
  """
  Cursor on `t` in `true` (line 63, col 36) — third argument of `multi`.
  Expects signature for `multi` with activeParameter=2.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/multi_param2"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(63, 36, _SigHelpChecker("multi", 2))])

class \nodoc\ iso _SigHelpNotInCallTest is UnitTest
  """
  Cursor on `s` in `simple` (line 48, col 10) — method declaration, not a call.
  Not inside a call expression — expects null / no signatures.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/not_in_call"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(48, 10, _SigHelpChecker(None, 0))])

class \nodoc\ iso _SigHelpNoArgsTest is UnitTest
  """
  Cursor on `s` in `no_args` (line 64, col 24) — call with no parameters.
  Expects signature for `no_args` with no activeParameter field (omitted for
  zero-param signatures per LSP 3.17 spec).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/no_args"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(64, 24, _SigHelpChecker("no_args", None))])

class \nodoc\ iso _SigHelpConstructorTest is UnitTest
  """
  Cursor on `0` (line 62, col 15) in `_SigTarget(0)` — a constructor call.
  Expects signature for `create` with activeParameter=0.
  Exercises the tk_new keyword branch in SignatureHelp.collect.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/constructor"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(62, 15, _SigHelpChecker("create", 0, None, "v:"))])

class \nodoc\ iso _SigHelpConstructorOpenParenTest is UnitTest
  """
  Cursor on `(` (line 62, col 14) in `_SigTarget(0)` — the open paren is
  before the first argument, so no signature help is returned.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/constructor_open_paren"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(62, 14, _SigHelpChecker(None))])

class \nodoc\ iso _SigHelpConstructorCloseParenTest is UnitTest
  """
  Cursor on `)` (line 62, col 16) in `_SigTarget(0).simple(42)` — the close
  paren is past the last argument of `_SigTarget(0)`, so no signature help
  is returned.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/constructor_close_paren"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(62, 16, _SigHelpChecker(None))])

class \nodoc\ iso _SigHelpCalleeTest is UnitTest
  """
  Four cursor positions on the callee of `.simple(42)` in line 62 — none
  inside the argument list, so all expect null / no signatures.

  (62, 17) — `.`          the dot before the method name
  (62, 18) — `s`          first character of method name
  (62, 23) — `e`          last  character of method name
  (62, 24) — `(`          open paren, before any argument
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/callee"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [ (62, 17, _SigHelpChecker(None))
        (62, 18, _SigHelpChecker(None))
        (62, 23, _SigHelpChecker(None))
        (62, 24, _SigHelpChecker(None)) ])

class \nodoc\ iso _SigHelpDocstringTest is UnitTest
  """
  Cursor on `4` in `42` (line 62, col 25) inside `simple(42)`.
  Verifies that signatures[0].documentation.value contains the
  method's docstring ("Returns x plus the stored value.").
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/docstring"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(62, 25, _SigHelpChecker("simple", 0, "Returns x plus"))])

class \nodoc\ iso _SigHelpParamTextTest is UnitTest
  """
  Cursor on `h` in `"hello"` (line 63, col 28) — second arg of `multi`.
  Verifies that the activeParameter's label offsets into signatures[0].label
  point to text containing "b:" (the second parameter name).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/param_text"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(63, 28, _SigHelpChecker("multi", 1, None, "b:"))])

class \nodoc\ iso _SigHelpAfterCommaTest is UnitTest
  """
  Cursor on the space at (line 63, col 26) — immediately after the first `,`
  in `multi(1, "hello", true)`. This is where the cursor sits when VS Code
  triggers signatureHelp on the `,` character.

  The server scans forward from whitespace to the next argument token so
  the popup shows the correct parameter highlighted.
  Expects signature for `multi` with activeParameter=1.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/after_comma"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(63, 26, _SigHelpChecker("multi", 1))])

class \nodoc\ iso _SigHelpAfterCommaMultilineTest is UnitTest
  """
  Cursor at (line 66, col 8) — one past the trailing `,` on `      1,`,
  in the multi-line `multi(...)` call. The next argument `"hello"` is
  on the following line (line 67, col 6).
  Expects signature for `multi` with activeParameter=1.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/after_comma_multiline"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(66, 8, _SigHelpChecker("multi", 1))])

class \nodoc\ iso _SigHelpMultilineArg0Test is UnitTest
  """
  Cursor directly on `1` (line 66, col 6) — first arg of the multi-line call.
  Expects signature for `multi` with activeParameter=0.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/multiline_arg0"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(66, 6, _SigHelpChecker("multi", 0))])

class \nodoc\ iso _SigHelpMultilineArg1Test is UnitTest
  """
  Cursor directly on `"` in `"hello"` (line 67, col 6) — second arg of the
  multi-line call. Expects signature for `multi` with activeParameter=1.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/multiline_arg1"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(67, 6, _SigHelpChecker("multi", 1))])

class \nodoc\ iso _SigHelpMultilineArg2Test is UnitTest
  """
  Cursor directly on `t` in `true)` (line 68, col 6) — third arg of the
  multi-line call. Expects signature for `multi` with activeParameter=2.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/multiline_arg2"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(68, 6, _SigHelpChecker("multi", 2))])

class \nodoc\ iso _SigHelpBeMethodTest is UnitTest
  """
  Cursor on `"` in `"hello"` (line 88, col 12) — first arg of `greet` behavior.
  Exercises the tk_be branch in SignatureHelp.collect.
  Expects signature for `greet` with activeParameter=0.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/be_method"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(88, 12, _SigHelpChecker("greet", 0))])

class \nodoc\ iso _SigHelpAfterOpenParenMultilineTest is UnitTest
  """
  Cursor at (line 65, col 24) — one past the `(` on `    _SigTarget(0).multi(`.
  The `(` trigger character fires here; the first argument is on the next line.
  Expects signature for `multi` with activeParameter=0.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/after_open_paren_multiline"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(65, 24, _SigHelpChecker("multi", 0))])

class \nodoc\ iso _SigHelpNestedTest is UnitTest
  """
  Three cursor positions inside
  `_SigTarget(_SigTarget(0).simple(1)).multi(2, "nested", false)`
  (line 69, 0-indexed).

  Verifies that the server resolves the innermost enclosing call at each
  cursor position, and that positions on a callee or open-paren of an inner
  call fall through to the surrounding outer call's signature.

  (69, 15) — `_` of inner `_SigTarget`, first token of the outer
    constructor's argument: expects `create` (outer constructor),
    activeParameter=0.
  (69, 26) — `0` inside the innermost `_SigTarget(0)` constructor:
    expects `create` (inner constructor), activeParameter=0.
  (69, 28) — `.` before `simple`, the callee of `simple(1)`:
    inner call is skipped; enclosing outer constructor gives
    `create`, activeParameter=0.
  (69, 34) — `e`, last character of `simple` (still on the callee):
    same skip behaviour; `create`, activeParameter=0.
  (69, 35) — `(` of `simple(1)`, before its first argument:
    gate rejects `simple(1)`; outer constructor gives
    `create`, activeParameter=0.
  (69, 36) — `1` inside `simple(1)` (the outer constructor's argument):
    expects `simple`, activeParameter=0.
  (69, 46) — `2`, the first argument of the outermost `multi(...)` call:
    expects `multi`, activeParameter=0.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/nested"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [ (69, 15, _SigHelpChecker("create", 0))
        (69, 26, _SigHelpChecker("create", 0))
        (69, 28, _SigHelpChecker("create", 0))
        (69, 34, _SigHelpChecker("create", 0))
        (69, 35, _SigHelpChecker("create", 0))
        (69, 36, _SigHelpChecker("simple", 0))
        (69, 46, _SigHelpChecker("multi", 0)) ])

class \nodoc\ iso _SigHelpGenericTest is UnitTest
  """
  Cursor on first `_n` (line 108, col 36) inside
  `_GenericSigPrimitive.typed[U32](_n, _n)`.
  Exercises the type-parameter path in _build_label: "typed[" verifies that
  type params are included in the label.
  Explicit type args are required because Pony cannot infer method-level type
  parameters. After typecheck, expr_qualify transforms TK_QUALIFY into a
  TK_FUNREF node whose child(0) is the original inner TK_FUNREF; the
  implementation detects this and descends to the inner ref for definitions().
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/generic"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [(109, 36, _SigHelpChecker("typed[", 0))])

class \nodoc\ iso _SigHelpNamedArgTest is UnitTest
  """
  Four cursor positions across two named-arg calls in `_NamedArgUser.apply`.

  After typecheck the compiler resolves named args into positional order, so
  named-arg VALUES behave exactly like positional args and activeParameter is
  correct. Named-arg NAMES (the identifiers before `=`) are not in any arg
  slot and return no signature help.

  Line 133 (0-indexed): all-named —
    `multi(where a = U8(1), b = "hello", c = false)`
    (133, 41) — `b`, a named-arg name: expects no signature help.
    (133, 45) — `"hello"`, a named-arg value: expects `multi`,
      activeParameter=1.

  Line 134 (0-indexed): mixed — `multi(1 where b = "hello", c = false)`
    (134, 36) — `"hello"`, named-arg value for `b`: expects `multi`,
      activeParameter=1.
    (134, 32) — `b`, a named-arg name: expects no signature help.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "signature_help/integration/named_arg"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "signature_help/signature_help.pony",
      [ (133, 41, _SigHelpChecker(None))
        (133, 45, _SigHelpChecker("multi", 1))
        (134, 36, _SigHelpChecker("multi", 1))
        (134, 32, _SigHelpChecker(None)) ])

class val _SigHelpChecker
  """
  Validates a textDocument/signatureHelp response.
  When expected_label_fragment is None, expects a null result (no call context).
  Otherwise checks that signatures[0].label contains the fragment and that
  activeParameter equals expected_active_param.

  Optional: expected_doc_fragment checks that
  signatures[0].documentation.value contains the given string.

  Optional: expected_param_text checks that the byte offsets in
  signatures[0].parameters[activeParameter].label point to text
  in the label containing the given string.

  Optional: expected_full_label asserts exact label equality, catching
  regressions in capability, parentheses, parameter list, or return type.
  """
  let _expected_label_fragment: (String val | None)
  let _expected_active_param: (I64 | None)
  let _expected_doc_fragment: (String val | None)
  let _expected_param_text: (String val | None)
  let _expected_full_label: (String val | None)

  new val create(
    expected_label_fragment': (String val | None),
    expected_active_param': (I64 | None) = 0,
    expected_doc_fragment': (String val | None) = None,
    expected_param_text': (String val | None) = None,
    expected_full_label': (String val | None) = None)
  =>
    _expected_label_fragment = expected_label_fragment'
    _expected_active_param = expected_active_param'
    _expected_doc_fragment = expected_doc_fragment'
    _expected_param_text = expected_param_text'
    _expected_full_label = expected_full_label'

  fun lsp_method(): String =>
    Methods.text_document().signature_help()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    match \exhaustive\ _expected_label_fragment
    | None =>
      // Expect no signatures (null result or empty signatures array)
      try
        let sigs = JsonNav(res.result)("signatures").as_array()?
        if sigs.size() > 0 then
          ok = false
          h.log("Expected no signature help but got: " + res.string())
        end
      end
    | let expected_label: String val =>
      let sigs =
        try JsonNav(res.result)("signatures").as_array()?
        else
          h.fail(
            "signatureHelp response missing signatures[] for '" +
            expected_label + "': " + res.string())
          return false
        end
      if not h.assert_true(sigs.size() > 0, "Expected signatures array") then
        return false
      end
      let label =
        try JsonNav(sigs(0)?)("label").as_string()?
        else
          h.fail(
            "signatureHelp response missing signatures[0].label for '" +
            expected_label + "': " + res.string())
          return false
        end
      if not h.assert_true(
        label.contains(expected_label),
        "Expected label to contain '" + expected_label + "', got: " + label)
      then
        ok = false
      end
      match \exhaustive\ _expected_full_label
      | let full_label: String val =>
        if not h.assert_eq[String](
          full_label,
          label,
          "Expected exact label '" + full_label + "', got: " + label)
        then
          ok = false
        end
      | None => None
      end
      match \exhaustive\ _expected_active_param
      | None =>
        // Zero-param signature: activeParameter must be absent.
        try
          let unexpected_active =
            JsonNav(res.result)("activeParameter").as_i64()?
          if not h.assert_true(
            false,
            "Expected activeParameter absent for zero-param signature, got: " +
            unexpected_active.string())
          then
            ok = false
          end
        end
      | let expected_active: I64 =>
        let active =
          try JsonNav(res.result)("activeParameter").as_i64()?
          else
            h.fail(
              "signatureHelp response missing activeParameter for '" +
              expected_label + "': " + res.string())
            return false
          end
        if not h.assert_eq[I64](
          expected_active,
          active,
          "activeParameter: expected " + expected_active.string() +
          ", got " + active.string())
        then
          ok = false
        end
      end
      match \exhaustive\ _expected_doc_fragment
      | let expected_doc: String val =>
        try
          let doc_val =
            JsonNav(sigs(0)?)("documentation")("value").as_string()?
          if not h.assert_true(
            doc_val.contains(expected_doc),
            "Expected doc to contain '" + expected_doc +
            "', got: " + doc_val)
          then
            ok = false
          end
        else
          ok = false
          h.log("Expected documentation field in: " + res.string())
        end
      | None => None
      end
      match \exhaustive\ _expected_param_text
      | let expected_text: String val =>
        try
          let params_arr =
            JsonNav(sigs(0)?)("parameters").as_array()?
          let active_idx =
            JsonNav(res.result)("activeParameter").as_i64()?.usize()
          let param_offsets =
            JsonNav(params_arr(active_idx)?)("label").as_array()?
          let p_start = JsonNav(param_offsets(0)?).as_i64()?
          let p_end = JsonNav(param_offsets(1)?).as_i64()?
          let param_text: String val =
            label.substring(p_start.isize(), p_end.isize())
          if not h.assert_true(
            param_text.contains(expected_text),
            "Expected param text to contain '" + expected_text +
            "', got: '" + param_text + "'")
          then
            ok = false
          end
        else
          ok = false
          h.log(
            "Expected parameter offsets for '" + expected_text +
            "' in: " + res.string())
        end
      | None => None
      end
    end
    ok
