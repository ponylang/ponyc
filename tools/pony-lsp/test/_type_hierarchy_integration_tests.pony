use ".."
use "pony_test"
use "files"
use "json"

// Fixture layout — _t_hier_b.pony (0-indexed lines, 0-indexed cols):
// line  0: trait _THierA  name (0,6)..(0,13)   full (0,0)..(4,17)
// line  6: trait _THierB  name (6,6)..(6,13)   full (6,0)..(11,17)
// line 13: class _THierC  name (13,6)..(13,13) full (13,0)..(18,25)
//
// Fixture layout — _t_hier_isect.pony:
// line  0: trait _THierLeft   name (0,6)..(0,16)   full (0,0)..(4,17)
// line  6: trait _THierRight  name (6,6)..(6,17)   full (6,0)..(10,17)
// line 12: trait _THierIsect  name (12,6)..(12,17) full (12,0)..(19,17)
//
// Fixture layout — _t_hier_d.pony:
// line 0: trait _THierD  name (0,6)..(0,13)  full (0,0)..(6,17)
//
// Fixture layout — _t_hier_e.pony:
// line 0: class _THierE  name (0,6)..(0,13)  full (0,0)..(4,27)
// (_THierE is the last entity in its file; SiblingBound returns None so
// ASTSourceSpan includes synthesized constructor tokens, inflating by 2.)

primitive _TypeHierarchyIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_PrepareTypeHierarchyOnEntityTest.create(server))
    test(_PrepareTypeHierarchyOnNonEntityTest.create(server))
    test(_TypeHierarchySupertypesTest.create(server))
    test(_TypeHierarchySupertypesEmptyTest.create(server))
    test(_TypeHierarchySubtypesTest.create(server))
    test(_TypeHierarchySubtypesLeafTest.create(server))
    test(_TypeHierarchyIsectypeTest.create(server))
    test(_TypeHierarchySubtypesCrossFileTest.create(server))

class \nodoc\ iso _PrepareTypeHierarchyOnEntityTest is UnitTest
  """
  Cursor on _THierB name (line 6, col 6) — expects a one-element result
  with all TypeHierarchyItem fields verified.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_hierarchy/integration/prepare_entity"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_hier_b = "type_hierarchy/_t_hier_b.pony"
    _RunLspChecks(
      h,
      _server,
      t_hier_b,
      [ _PrepareTypeHierarchyChecker(
          (6, 6),
          _THierExpected(
            "_THierB",
            11,
            workspace_dir,
            t_hier_b,
            (6, 6, 6, 13),
            (6, 0, 11, 17))) ])

class \nodoc\ iso _PrepareTypeHierarchyOnNonEntityTest is UnitTest
  """
  Cursor on method keyword `fun` (line 4, col 2) — not an entity.
  Expects null result.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_hierarchy/integration/prepare_non_entity"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "type_hierarchy/_t_hier_b.pony",
      [_PrepareTypeHierarchyChecker((4, 2), None)])

class \nodoc\ iso _TypeHierarchySupertypesTest is UnitTest
  """
  Supertypes of _THierB (provides _THierA) — expects one item with all
  TypeHierarchyItem fields verified.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_hierarchy/integration/supertypes"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_hier_b = "type_hierarchy/_t_hier_b.pony"
    _RunLspChecks(
      h,
      _server,
      t_hier_b,
      [ _THierItemsChecker(
          Methods.type_hierarchy().supertypes(),
          workspace_dir,
          t_hier_b,
          (6, 6),
          [ _THierExpected(
              "_THierA",
              11,
              workspace_dir,
              t_hier_b,
              (0, 6, 0, 13),
              (0, 0, 4, 17)) ]) ])

class \nodoc\ iso _TypeHierarchySupertypesEmptyTest is UnitTest
  """
  Supertypes of _THierA (no provides) — expects an empty array (not null).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_hierarchy/integration/supertypes_empty"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_hier_b = "type_hierarchy/_t_hier_b.pony"
    _RunLspChecks(
      h,
      _server,
      t_hier_b,
      [ _THierItemsChecker(
          Methods.type_hierarchy().supertypes(),
          workspace_dir,
          t_hier_b,
          (0, 6),
          []) ])

class \nodoc\ iso _TypeHierarchySubtypesTest is UnitTest
  """
  Subtypes of _THierA — expects one item: _THierB.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_hierarchy/integration/subtypes"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_hier_b = "type_hierarchy/_t_hier_b.pony"
    _RunLspChecks(
      h,
      _server,
      t_hier_b,
      [ _THierItemsChecker(
          Methods.type_hierarchy().subtypes(),
          workspace_dir,
          t_hier_b,
          (0, 6),
          [ _THierExpected(
              "_THierB",
              11,
              workspace_dir,
              t_hier_b,
              (6, 6, 6, 13),
              (6, 0, 11, 17)) ]) ])

class \nodoc\ iso _TypeHierarchySubtypesLeafTest is UnitTest
  """
  Subtypes of _THierC (a leaf class) — expects an empty array (not null).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_hierarchy/integration/subtypes_leaf"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_hier_b = "type_hierarchy/_t_hier_b.pony"
    _RunLspChecks(
      h,
      _server,
      t_hier_b,
      [ _THierItemsChecker(
          Methods.type_hierarchy().subtypes(),
          workspace_dir,
          t_hier_b,
          (13, 6),
          []) ])

class \nodoc\ iso _TypeHierarchyIsectypeTest is UnitTest
  """
  Supertypes of _THierIsect (provides _THierLeft & _THierRight via
  intersection type) — verifies the tk_isecttype recursion path in
  _collect_provides_defs.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_hierarchy/integration/supertypes_isecttype"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_hier_isect = "type_hierarchy/_t_hier_isect.pony"
    _RunLspChecks(
      h,
      _server,
      t_hier_isect,
      [ _THierItemsChecker(
          Methods.type_hierarchy().supertypes(),
          workspace_dir,
          t_hier_isect,
          (12, 6),
          [ _THierExpected(
              "_THierLeft",
              11,
              workspace_dir,
              t_hier_isect,
              (0, 6, 0, 16),
              (0, 0, 4, 17))
            _THierExpected(
              "_THierRight",
              11,
              workspace_dir,
              t_hier_isect,
              (6, 6, 6, 17),
              (6, 0, 10, 17)) ]) ])

class \nodoc\ iso _TypeHierarchySubtypesCrossFileTest is UnitTest
  """
  Subtypes of _THierD — expects _THierE from a different source file,
  verifying the cross-module walk in _SubtypeCollector.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_hierarchy/integration/subtypes_cross_file"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_hier_d = "type_hierarchy/_t_hier_d.pony"
    let t_hier_e = "type_hierarchy/_t_hier_e.pony"
    _RunLspChecks(
      h,
      _server,
      t_hier_d,
      [ _THierItemsChecker(
          Methods.type_hierarchy().subtypes(),
          workspace_dir,
          t_hier_d,
          (0, 6),
          [ _THierExpected(
              "_THierE",
              5,
              workspace_dir,
              t_hier_e,
              (0, 6, 0, 13),
              (0, 0, 4, 27)) ]) ])

class val _THierExpected
  """
  Expected TypeHierarchyItem field values for assertion in test checkers.
  sel: selectionRange (start_line, start_char, end_line, end_char).
  rng: full range (start_line, start_char, end_line, end_char).
  """
  let name: String val
  let kind: I64
  let uri: String val
  let sel: (I64, I64, I64, I64)
  let rng: (I64, I64, I64, I64)

  new val create(
    name': String,
    kind': I64,
    workspace_dir: String,
    workspace_file: String,
    sel': (I64, I64, I64, I64),
    rng': (I64, I64, I64, I64))
  =>
    name = name'
    kind = kind'
    uri = Uris.from_path(Path.join(workspace_dir, workspace_file))
    sel = sel'
    rng = rng'

class val _PrepareTypeHierarchyChecker
  """
  Checker for textDocument/prepareTypeHierarchy.
  The harness supplies textDocument.uri; lsp_params adds position.
  Pass None for expected to assert a null result (non-entity cursor).
  """
  let _sel_pos: (I64, I64)
  let _expected: (_THierExpected val | None)

  new val create(
    sel_pos': (I64, I64),
    expected: (_THierExpected val | None))
  =>
    _sel_pos = sel_pos'
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().prepare_type_hierarchy()

  fun lsp_params(): (None | JsonObject) =>
    JsonObject.update(
      "position",
      JsonObject
        .update("line", _sel_pos._1)
        .update("character", _sel_pos._2))

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match \exhaustive\ _expected
    | None =>
      match res.result
      | None => true
      else
        h.fail("prepare: expected null for non-entity cursor, got non-null")
        false
      end
    | let exp: _THierExpected val =>
      match res.result
      | let arr: JsonArray =>
        var ok =
          h.assert_eq[USize](
            1,
            try JsonNav(arr).size()? else 0 end,
            "prepare: expected single item")
        ok = _THierCheckItem(JsonNav(arr), 0, exp, h) and ok
        ok
      | None =>
        h.fail("prepare: expected item, got null")
        false
      else
        h.fail("prepare: expected array result")
        false
      end
    end

class val _THierItemsChecker
  """
  Checker for typeHierarchy/supertypes and typeHierarchy/subtypes.
  Asserts the result is a JsonArray (not null), verifies item count, and
  checks all TypeHierarchyItem fields: name, kind, uri, selectionRange,
  range.
  """
  let _method: String
  let _item_uri: String val
  let _sel_pos: (I64, I64)
  let _expected: Array[_THierExpected val] val

  new val create(
    method': String,
    workspace_dir: String,
    item_file: String,
    sel_pos': (I64, I64),
    expected: Array[_THierExpected val] val)
  =>
    _method = method'
    _item_uri = Uris.from_path(Path.join(workspace_dir, item_file))
    _sel_pos = sel_pos'
    _expected = expected

  fun lsp_method(): String => _method

  fun lsp_params(): (None | JsonObject) =>
    JsonObject.update(
      "item",
      JsonObject
        .update("uri", _item_uri)
        .update(
          "selectionRange",
          JsonObject.update(
            "start",
            JsonObject
              .update("line", _sel_pos._1)
              .update("character", _sel_pos._2))))

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | None =>
      if _expected.size() == 0 then
        h.fail(_method + ": expected [] not null for empty result")
      else
        h.fail(_method + ": expected items, got null")
      end
      false
    | let arr: JsonArray =>
      let nav = JsonNav(arr)
      var ok =
        h.assert_eq[USize](
          _expected.size(),
          try nav.size()? else 0 end,
          _method + ": wrong item count")
      for (i, exp) in _expected.pairs() do
        ok = _THierCheckItem(nav, i, exp, h) and ok
      end
      ok
    else
      h.fail(_method + ": expected array result")
      false
    end

primitive _THierCheckItem
  """
  Asserts all TypeHierarchyItem fields of the item at index i in nav.
  """
  fun apply(
    nav: JsonNav box,
    i: USize,
    exp: _THierExpected val,
    h: TestHelper): Bool
  =>
    try
      var ok = true
      ok = h.assert_eq[String](
        exp.name,
        nav(i)("name").as_string()?,
        "item[" + i.string() + "].name") and ok
      ok = h.assert_eq[I64](
        exp.kind,
        nav(i)("kind").as_i64()?,
        "item[" + i.string() + "].kind") and ok
      ok = h.assert_eq[String](
        exp.uri,
        nav(i)("uri").as_string()?,
        "item[" + i.string() + "].uri") and ok
      ok = h.assert_eq[I64](
        exp.sel._1,
        nav(i)("selectionRange")("start")("line").as_i64()?,
        "item[" + i.string() + "].selectionRange.start.line") and ok
      ok = h.assert_eq[I64](
        exp.sel._2,
        nav(i)("selectionRange")("start")("character").as_i64()?,
        "item[" + i.string() + "].selectionRange.start.character") and ok
      ok = h.assert_eq[I64](
        exp.sel._3,
        nav(i)("selectionRange")("end")("line").as_i64()?,
        "item[" + i.string() + "].selectionRange.end.line") and ok
      ok = h.assert_eq[I64](
        exp.sel._4,
        nav(i)("selectionRange")("end")("character").as_i64()?,
        "item[" + i.string() + "].selectionRange.end.character") and ok
      ok = h.assert_eq[I64](
        exp.rng._1,
        nav(i)("range")("start")("line").as_i64()?,
        "item[" + i.string() + "].range.start.line") and ok
      ok = h.assert_eq[I64](
        exp.rng._2,
        nav(i)("range")("start")("character").as_i64()?,
        "item[" + i.string() + "].range.start.character") and ok
      ok = h.assert_eq[I64](
        exp.rng._3,
        nav(i)("range")("end")("line").as_i64()?,
        "item[" + i.string() + "].range.end.line") and ok
      ok = h.assert_eq[I64](
        exp.rng._4,
        nav(i)("range")("end")("character").as_i64()?,
        "item[" + i.string() + "].range.end.character") and ok
      ok
    else
      h.fail(
        "item[" + i.string() + "] missing or has wrong structure"
          + " (expected: " + exp.name + ")")
      false
    end
