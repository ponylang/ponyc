use ".."
use "pony_test"
use "files"
use "json"

primitive _DocumentSymbolIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    // All fixtures live under workspace/document_symbol/ so the whole
    // suite compiles a single package once (the test server's first-
    // `publish_diagnostics` gate only drains pending requests on the
    // initial compile — a mid-suite package switch would race the new
    // compile and hand documentSymbol requests an empty module).
    test(_DocSymContainmentTest.create(server))
    test(_DocSymRangeTest.create(server))
    test(_DocSymEntityKindsTest.create(server))
    test(_DocSymMemberKindsTest.create(server))
    test(_DocSymCrossFileTraitTest.create(server))
    test(_DocSymImplNoChildrenTest.create(server))
    test(_DocSymMixedChildrenTest.create(server))
    test(_DocSymTypeAliasRangeTest.create(server))
    test(_DocSymPrimitiveRangeTest.create(server))

class \nodoc\ iso _DocSymContainmentTest is UnitTest
  """
  Regression guard for #5240 follow-up: every symbol's selectionRange must
  be contained within its range. Checks the top-level symbols and all
  nested children recursively.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/containment"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_containment.pony",
      [_DocSymContainmentChecker])

class \nodoc\ iso _DocSymRangeTest is UnitTest
  """
  Verifies that documentSymbol returns the full declaration range for
  two representative symbols, asserting all four coordinates of both
  `range` and `selectionRange` exactly.

  "_DsContainment" — `range` must span from the `class` keyword to the
  end of the class body; `selectionRange` covers the identifier only.

  "increment" — `range` must span from the `fun` keyword to the end of
  the method body; `selectionRange` covers the identifier only.

  Exact position tuples are derived from the fixture layout in
  `document_symbol/_ds_containment.pony`. Reformatting that file
  invalidates the tuples below.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/range"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_containment.pony",
      [ _DocSymRangeChecker(
            "_DsContainment",
            None,
                      // range: (start_line, start_char, end_line, end_char)
            (0, 0, 18, 10),
            // selectionRange: (start_line, start_char, end_line, end_char)
            (0, 6, 0, 20))
        _DocSymRangeChecker(
            "increment",
            "_DsContainment",
            (13, 2, 15, 10),
            (13, 10, 13, 19))])

class \nodoc\ iso _DocSymEntityKindsTest is UnitTest
  """
  Asserts that every Pony entity token surfaces as a top-level
  DocumentSymbol with the LSP SymbolKind it is mapped to by
  `DocumentSymbols.from_module`:
    class, actor, primitive, type  → sk_class (5)
    trait, interface               → sk_interface (11)
    struct                         → sk_struct (23)

  Regression guard against silent edits to the `tk_*` → SymbolKind
  match table at `tools/pony-lsp/symbols.pony:153-163`.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/entity_kinds"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_ent_interface.pony",
      [ _DocSymTopLevelKindsChecker(
            [ ("_DsEntClass", 5)
              ("_DsEntActor", 5)
              ("_DsEntTrait", 11)
              ("_DsEntInterface", 11)
              ("_DsEntPrimitive", 5)
              ("_DsEntStruct", 23)
              ("_DsEntType", 5)])])

class \nodoc\ iso _DocSymMemberKindsTest is UnitTest
  """
  Asserts that every member token surfaces as a child DocumentSymbol
  under its enclosing entity with the LSP SymbolKind it is mapped to
  by `DocumentSymbols.find_members`:
    tk_new                         → constructor (9)
    tk_fun, tk_be                  → method (6)
    tk_flet, tk_fvar, tk_embed     → field (8)

  Regression guard against silent edits to the member `tk_*` →
  SymbolKind match table at `tools/pony-lsp/symbols.pony:214-220`.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/member_kinds"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_member_host.pony",
      [ _DocSymChildKindsChecker(
            "_DsMemberHost",
            [ ("_let_field", 8)
              ("_var_field", 8)
              ("_embed_field", 8)
              ("create", 9)
              ("ds_fun", 6)
              ("ds_be", 6)])])

class \nodoc\ iso _DocSymCrossFileTraitTest is UnitTest
  """
  Regression guard for the source-file filter in `ASTSourceSpan`.

  `_DsImpl` (in `_ds_impl.pony`) inherits `ds_default_method` from
  `_DsTrait` (in `_ds_trait.pony`) without overriding it. ponyc merges
  the trait's default body into the impl's AST, but the merged tokens
  carry the trait file's `source_file()`. `ASTSourceSpan` filters those
  descendants out when computing the impl's span.

  If the filter regresses, `_DsImpl.range.end.line` would jump to a line
  in the trait file (past line 20). The assertion below caps end.line
  well below that, so any drift into the trait file is detected.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/cross_file_trait"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_impl.pony",
      [ _DocSymMaxEndLineChecker("_DsImpl", 15)])

class \nodoc\ iso _DocSymImplNoChildrenTest is UnitTest
  """
  Regression guard for synthesized and trait-merged members leaking into
  the outline.

  `_DsImpl` (in `_ds_impl.pony`) has no explicitly written members: it
  inherits `ds_default_method` from `_DsTrait` (merged by ponyc) and
  receives a synthesized `create` from the sugar pass. Both must be
  filtered out by `DocumentSymbols.find_members`; the symbol's children
  array must be empty.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/impl_no_children"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_impl.pony",
      [_DocSymNoChildrenChecker("_DsImpl")])

class \nodoc\ iso _DocSymMixedChildrenTest is UnitTest
  """
  Verifies that the position filter suppresses the synthesized `create`
  constructor without over-suppressing explicitly written members.

  `_DsMixed` (in `_ds_mixed.pony`) has no explicit `new`, so ponyc's
  sugar pass synthesizes a `create` at the class keyword's position.
  It has one explicitly written `fun ds_mixed_method`. The outline must
  include the explicit method and exclude the synthesized constructor —
  exactly one child with the right name and kind.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/mixed_children"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_mixed.pony",
      [_DocSymChildKindsChecker("_DsMixed", [("ds_mixed_method", 6)])])

class \nodoc\ iso _DocSymTypeAliasRangeTest is UnitTest
  """
  Verifies that a `type` alias has a range covering the entire
  declaration — from the `type` keyword to the end of the nominal
  reference — and a selectionRange covering only the identifier.

  Regression guard for cross-entity bleed: type aliases hold usage-site
  positions for the nominal reference, so `ASTSourceSpan` must not drift
  into the referenced entity's tokens.

  Exact positions are derived from `_ds_ent_interface.pony` line 23
  (1-based) = line 22 (0-based):
    `type _DsEntType is _DsEntClass`
     ^0                            ^30
          ^5        ^15
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/type_alias_range"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_ent_interface.pony",
      [ _DocSymRangeChecker(
            "_DsEntType",
            None,
            (22, 0, 22, 30),
            (22, 5, 22, 15))])

class \nodoc\ iso _DocSymPrimitiveRangeTest is UnitTest
  """
  Verifies that a bare `primitive` (no explicit body) has a range
  covering only the declaration line — from the `primitive` keyword to
  the end of the identifier — and a selectionRange covering only the
  identifier.

  Exact positions are derived from `_ds_ent_interface.pony` line 18
  (1-based) = line 17 (0-based):
    `primitive _DsEntPrimitive`
     ^0                       ^25
               ^10            ^25
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "document_symbol/integration/primitive_range"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "document_symbol/_ds_ent_interface.pony",
      [ _DocSymRangeChecker(
          "_DsEntPrimitive",
          None,
          (17, 0, 17, 25),
          (17, 10, 17, 25))])

class val _DocSymContainmentChecker
  """
  Validates that every DocumentSymbol (and every nested child) in a
  textDocument/documentSymbol response has selectionRange contained
  within range.
  """
  new val create() => None

  fun lsp_method(): String =>
    Methods.text_document().document_symbol()

  fun lsp_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      var ok = true
      for item in arr.values() do
        ok = _check_symbol(item, h) and ok
      end
      ok
    else
      h.fail("documentSymbol: expected array result, got null")
      false
    end

  fun _check_symbol(item: JsonValue, h: TestHelper): Bool =>
    var ok = true
    try
      let name = JsonNav(item)("name").as_string()?
      let r_sl = JsonNav(item)("range")("start")("line").as_i64()?
      let r_sc = JsonNav(item)("range")("start")("character").as_i64()?
      let r_el = JsonNav(item)("range")("end")("line").as_i64()?
      let r_ec = JsonNav(item)("range")("end")("character").as_i64()?
      let s_sl = JsonNav(item)("selectionRange")("start")("line").as_i64()?
      let s_sc = JsonNav(item)("selectionRange")("start")("character").as_i64()?
      let s_el = JsonNav(item)("selectionRange")("end")("line").as_i64()?
      let s_ec = JsonNav(item)("selectionRange")("end")("character").as_i64()?

      // selectionRange.start must be >= range.start
      let start_ok =
        (s_sl > r_sl) or ((s_sl == r_sl) and (s_sc >= r_sc))
      if not h.assert_true(
        start_ok,
        "documentSymbol '" + name +
        "': selectionRange.start (" + s_sl.string() + ":" + s_sc.string() +
        ") must be >= range.start (" + r_sl.string() + ":" + r_sc.string() +
        ")")
      then
        ok = false
      end

      // selectionRange.end must be <= range.end
      let end_ok =
        (s_el < r_el) or ((s_el == r_el) and (s_ec <= r_ec))
      if not h.assert_true(
        end_ok,
        "documentSymbol '" + name +
        "': selectionRange.end (" + s_el.string() + ":" + s_ec.string() +
        ") must be <= range.end (" + r_el.string() + ":" + r_ec.string() +
        ")")
      then
        ok = false
      end

      // Recurse into children if present.
      try
        let children = JsonNav(item)("children").as_array()?
        for child in children.values() do
          ok = _check_symbol(child, h) and ok
        end
      end
    else
      h.fail("documentSymbol: malformed symbol entry")
      ok = false
    end
    ok

class val _DocSymRangeChecker
  """
  Validates that a named symbol (optionally nested under a named parent)
  in a documentSymbol response has the expected exact `range` and
  `selectionRange` coordinates. Asserting all four coordinates of both
  fields catches regressions that collapse a range to `{0,0}-{0,0}` or
  that set `range` to a keyword-only span.
  """
  let _symbol_name: String
  let _parent_name: (String | None)
  let _range: (I64, I64, I64, I64)
  let _selection_range: (I64, I64, I64, I64)

  new val create(
    symbol_name: String,
    parent_name: (String | None),
    range': (I64, I64, I64, I64),
    selection_range': (I64, I64, I64, I64))
  =>
    _symbol_name = symbol_name
    _parent_name = parent_name
    _range = range'
    _selection_range = selection_range'

  fun lsp_method(): String =>
    Methods.text_document().document_symbol()

  fun lsp_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      let found = _find(arr, _symbol_name, _parent_name)
      match found
      | None =>
        h.fail(
          "documentSymbol: symbol '" + _symbol_name +
          "' not found in response")
        false
      else
        _assert_ranges(found, h)
      end
    else
      h.fail("documentSymbol: expected array result, got null")
      false
    end

  fun _find(
    arr: JsonArray,
    name: String,
    parent: (String | None))
    : JsonValue
  =>
    for item in arr.values() do
      try
        let item_name = JsonNav(item)("name").as_string()?
        match \exhaustive\ parent
        | None =>
          if item_name == name then return item end
        | let p: String =>
          if item_name == p then
            try
              let children = JsonNav(item)("children").as_array()?
              for child in children.values() do
                try
                  if JsonNav(child)("name").as_string()? == name then
                    return child
                  end
                end
              end
            end
          end
        end
      end
    end
    None

  fun _assert_ranges(item: JsonValue, h: TestHelper): Bool =>
    (let exp_r_sl, let exp_r_sc, let exp_r_el, let exp_r_ec) = _range
    (let exp_s_sl, let exp_s_sc, let exp_s_el, let exp_s_ec) = _selection_range
    var ok = true
    try
      let r_sl = JsonNav(item)("range")("start")("line").as_i64()?
      let r_sc = JsonNav(item)("range")("start")("character").as_i64()?
      let r_el = JsonNav(item)("range")("end")("line").as_i64()?
      let r_ec = JsonNav(item)("range")("end")("character").as_i64()?
      let s_sl = JsonNav(item)("selectionRange")("start")("line").as_i64()?
      let s_sc = JsonNav(item)("selectionRange")("start")("character").as_i64()?
      let s_el = JsonNav(item)("selectionRange")("end")("line").as_i64()?
      let s_ec = JsonNav(item)("selectionRange")("end")("character").as_i64()?

      ok = h.assert_eq[I64](
        exp_r_sl, r_sl, _symbol_name + ": range.start.line") and ok
      ok = h.assert_eq[I64](
        exp_r_sc, r_sc, _symbol_name + ": range.start.character") and ok
      ok = h.assert_eq[I64](
        exp_r_el, r_el, _symbol_name + ": range.end.line") and ok
      ok = h.assert_eq[I64](
        exp_r_ec, r_ec, _symbol_name + ": range.end.character") and ok
      ok = h.assert_eq[I64](
        exp_s_sl, s_sl, _symbol_name + ": selectionRange.start.line") and ok
      ok = h.assert_eq[I64](
        exp_s_sc,
        s_sc,
        _symbol_name + ": selectionRange.start.character") and ok
      ok = h.assert_eq[I64](
        exp_s_el, s_el, _symbol_name + ": selectionRange.end.line") and ok
      ok = h.assert_eq[I64](
        exp_s_ec, s_ec, _symbol_name + ": selectionRange.end.character") and ok
    else
      h.fail(_symbol_name + ": malformed symbol entry")
      ok = false
    end
    ok

class val _DocSymTopLevelKindsChecker
  """
  Validates that the documentSymbol response contains exactly the
  expected list of top-level (name, kind) pairs, in any order.
  """
  let _expected: Array[(String, I64)] val

  new val create(expected: Array[(String, I64)] val) =>
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().document_symbol()

  fun lsp_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    match res.result
    | let arr: JsonArray =>
      ok = h.assert_eq[USize](
        _expected.size(),
        arr.size(),
        "documentSymbol: top-level count") and ok
      for (exp_name, exp_kind) in _expected.values() do
        var found = false
        for item in arr.values() do
          try
            let n = JsonNav(item)("name").as_string()?
            let k = JsonNav(item)("kind").as_i64()?
            if (n == exp_name) and (k == exp_kind) then
              found = true
              break
            end
          end
        end
        ok = h.assert_true(
          found,
          "documentSymbol: expected top-level symbol '" + exp_name +
          "' with kind " + exp_kind.string() + " not found") and ok
      end
    else
      h.fail("documentSymbol: expected array result, got null")
      ok = false
    end
    ok

class val _DocSymChildKindsChecker
  """
  Finds `_parent_name` at the top level of the documentSymbol response,
  then validates that its `children` array contains exactly the
  expected list of (name, kind) pairs, in any order.
  """
  let _parent_name: String
  let _expected: Array[(String, I64)] val

  new val create(
    parent_name: String,
    expected: Array[(String, I64)] val)
  =>
    _parent_name = parent_name
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().document_symbol()

  fun lsp_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      for item in arr.values() do
        try
          if JsonNav(item)("name").as_string()? == _parent_name then
            return _check_children(item, h)
          end
        end
      end
      h.fail(
        "documentSymbol: parent '" + _parent_name + "' not found")
      false
    else
      h.fail("documentSymbol: expected array result, got null")
      false
    end

  fun _check_children(parent: JsonValue, h: TestHelper): Bool =>
    var ok = true
    try
      let children = JsonNav(parent)("children").as_array()?
      ok = h.assert_eq[USize](
        _expected.size(),
        children.size(),
        "documentSymbol '" + _parent_name + "': children count") and ok
      for (exp_name, exp_kind) in _expected.values() do
        var found = false
        for child in children.values() do
          try
            let n = JsonNav(child)("name").as_string()?
            let k = JsonNav(child)("kind").as_i64()?
            if (n == exp_name) and (k == exp_kind) then
              found = true
              break
            end
          end
        end
        ok = h.assert_true(
          found,
          "documentSymbol '" + _parent_name +
          "': expected child '" + exp_name + "' with kind " +
          exp_kind.string() + " not found") and ok
      end
    else
      h.fail(
        "documentSymbol '" + _parent_name +
        "': children array missing")
      ok = false
    end
    ok

class val _DocSymMaxEndLineChecker
  """
  Asserts that a named top-level symbol's range.end.line is strictly
  less than `_max_end_line`. Used to detect source-file filter
  regressions where the range inflates to include descendants from
  other files.
  """
  let _symbol_name: String
  let _max_end_line: I64

  new val create(symbol_name: String, max_end_line: I64) =>
    _symbol_name = symbol_name
    _max_end_line = max_end_line

  fun lsp_method(): String =>
    Methods.text_document().document_symbol()

  fun lsp_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      for item in arr.values() do
        try
          if JsonNav(item)("name").as_string()? == _symbol_name then
            let end_line =
              JsonNav(item)("range")("end")("line").as_i64()?
            return h.assert_true(
              end_line < _max_end_line,
              _symbol_name +
              ": range.end.line (" + end_line.string() +
              ") should be < " + _max_end_line.string() +
              " (source-file filter)")
          end
        end
      end
      h.fail(
        "documentSymbol: symbol '" + _symbol_name + "' not found")
      false
    else
      h.fail("documentSymbol: expected array result, got null")
      false
    end

class val _DocSymNoChildrenChecker
  """
  Asserts that a named top-level symbol has no children in the
  documentSymbol response. Used to verify that synthesized constructors
  (ponyc sugar) and trait-merged methods are not included in the outline.
  """
  let _symbol_name: String

  new val create(symbol_name: String) =>
    _symbol_name = symbol_name

  fun lsp_method(): String =>
    Methods.text_document().document_symbol()

  fun lsp_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      for item in arr.values() do
        try
          if JsonNav(item)("name").as_string()? == _symbol_name then
            // children key is omitted from JSON when the array is empty;
            // if present it must be empty.
            try
              let children = JsonNav(item)("children").as_array()?
              return h.assert_eq[USize](
                0,
                children.size(),
                _symbol_name + ": expected no children in outline")
            end
            return true
          end
        end
      end
      h.fail("documentSymbol: symbol '" + _symbol_name + "' not found")
      false
    else
      h.fail("documentSymbol: expected array result, got null")
      false
    end
