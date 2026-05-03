use ".."
use "pony_test"
use "files"
use "json"

// Fixture layout (0-indexed lines, 0-indexed cols):
// _t_call_a.pony  class _TCallA
// f_a  name (1,6)..(1,9)  full (1,2)..(1,27)  [end inflated: None desugars]
// _t_call_b.pony  class _TCallB
// f_b  name (1,6)..(1,9)  full (1,2)..(2,10)  [end at '(' - ')' not tracked]
// a.f_a()  call site f_a (2,6)..(2,9)
// _t_call_c.pony  class _TCallC
// f_c  name (1,6)..(1,9)  full (1,2)..(3,11)  [end at last arg, not ')']
// a.f_a()  call site f_a (2,6)..(2,9)
// b.f_b(a)  call site f_b (3,6)..(3,9)
// _t_call_d.pony  class _TCallD
// f_d  name (1,6)..(1,9)  full (1,2)..(3,10)
// a.f_a()  first call site f_a (2,6)..(2,9)
// a.f_a()  second call site f_a (3,6)..(3,9)
// _t_call_actor.pony  actor _TCallActor
// create  new, name (1,6)..(1,12)  kind=9 (constructor)
// act  be, name (4,5)..(4,8)  full (4,2)..(5,10)
// a.f_a()  call site f_a (5,6)..(5,9)
// _t_call_lambda.pony  class _TCallLambda
// f_lambda  name (1,6)..(1,14)  full (1,2)..(9,10)
// [end inflated: None desugars]
// a.f_a()  first call site f_a (2,6)..(2,9)
// a.f_a()  second call site f_a (8,6)..(8,9) — after the object literal
// object literal g() has no calls  [nested method; excluded by _nested_depth]
// _t_poly.pony  _TPolyA, _TPolyB, _TPolyUser (same-name method, two entities)
// _TPolyA.poly_method  name (1,6)..(1,17)  full (1,2)..(1,35)
// [end inflated: None desugars]
// _TPolyB.poly_method  name (4,6)..(4,17)  full (4,2)..(4,35)
// [end inflated: None desugars]
// _TPolyUser.call_a  name (7,6)..(7,12)  full (7,2)..(8,18)
// a.poly_method() call site poly_method (8,6)..(8,17)
// _TPolyUser.call_b  name (10,6)..(10,12)  full (10,2)..(11,18)
// b.poly_method() call site poly_method (11,6)..(11,17)
// _t_cross_caller.pony  class _TCrossCaller (uses "callee" sub-package)
// cross_call  name (3,6)..(3,16)  full (3,2)..(4,20)
// c.callee_method() call site callee_method (4,6)..(4,19)
// callee/t_callee.pony  TCallee (public; sub-package; no ".." import; no cycle)
// callee_method  name (4,6)..(4,19)  full (4,2)..(4,37)
// [end inflated: None desugars]
// _t_call_constructor.pony  classes _TConstructed, _TCallConstructorCaller,
// _TConstructorWithBody
// _TConstructed.create  new, name (1,6)..(1,12)  full (1,2)..(1,24)
// [end inflated: None desugars]
// _TCallConstructorCaller.f_new_explicit  name (4,6)..(4,20)
// _TConstructed.create()  create at (5,18)..(5,24)
// _TConstructorWithBody.create  new, name (9,6)..(9,12)  full (9,2)..(10,10)
// a.f_a()  call site f_a (10,6)..(10,9)

primitive _CallHierarchyIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_PrepareCallHierarchyOnMethodTest.create(server))
    test(_PrepareCallHierarchyOnNonMethodTest.create(server))
    test(_IncomingCallsForFaTest.create(server))
    test(_IncomingCallsEmptyTest.create(server))
    test(_OutgoingCallsEmptyTest.create(server))
    test(_OutgoingCallsForFbTest.create(server))
    test(_OutgoingCallsForFcTest.create(server))
    test(_OutgoingCallsForFdTest.create(server))
    test(_PrepareConstructorKindTest.create(server))
    test(_PrepareBeKindTest.create(server))
    test(_OutgoingCallsForActTest.create(server))
    test(_PrepareFromCallRefTest.create(server))
    test(_OutgoingCallsNoLambdaCallsTest.create(server))
    test(_IncomingCallsPolyATest.create(server))
    test(_IncomingCallsPolyBTest.create(server))
    test(_OutgoingCallsCrossPackageTest.create(server))
    test(_OutgoingCallsForFNewExplicitTest.create(server))
    test(_OutgoingCallsFromConstructorTest.create(server))

class val _CHierItem
  """
  Expected fields of a CallHierarchyItem.
  sel: selectionRange (sl, sc, el, ec); rng: full range (sl, sc, el, ec).
  """
  let name: String val
  let kind: I64
  let detail: String val
  let uri: String val
  let sel: (I64, I64, I64, I64)
  let rng: (I64, I64, I64, I64)

  new val create(
    name': String,
    kind': I64,
    detail': String,
    workspace_dir: String,
    workspace_file: String,
    sel': (I64, I64, I64, I64),
    rng': (I64, I64, I64, I64))
  =>
    name = name'
    kind = kind'
    detail = detail'
    uri = Uris.from_path(Path.join(workspace_dir, workspace_file))
    sel = sel'
    rng = rng'

class val _CHierExpected
  """
  Expected caller or callee entry in an incoming/outgoing call result.
  """
  let item: _CHierItem val
  let from_ranges: Array[(I64, I64, I64, I64)] val

  new val create(
    item': _CHierItem val,
    from_ranges': Array[(I64, I64, I64, I64)] val)
  =>
    item = item'
    from_ranges = from_ranges'

primitive _CHierCheckItem
  """
  Asserts all CallHierarchyItem fields from `item_nav` — a JsonNav pointing
  directly at the item object (not an array element).
  """
  fun apply(
    item_nav: JsonNav,
    exp: _CHierItem val,
    label: String val,
    h: TestHelper): Bool
  =>
    try
      var ok = true
      ok = h.assert_eq[String](
        exp.name,
        item_nav("name").as_string()?,
        label + ".name") and ok
      ok = h.assert_eq[I64](
        exp.kind,
        item_nav("kind").as_i64()?,
        label + ".kind") and ok
      ok = h.assert_eq[String](
        exp.uri,
        item_nav("uri").as_string()?,
        label + ".uri") and ok
      ok = h.assert_eq[String](
        exp.detail,
        try item_nav("detail").as_string()? else "" end,
        label + ".detail") and ok
      ok = h.assert_eq[I64](
        exp.sel._1,
        item_nav("selectionRange")("start")("line").as_i64()?,
        label + ".selectionRange.start.line") and ok
      ok = h.assert_eq[I64](
        exp.sel._2,
        item_nav("selectionRange")("start")("character").as_i64()?,
        label + ".selectionRange.start.character") and ok
      ok = h.assert_eq[I64](
        exp.sel._3,
        item_nav("selectionRange")("end")("line").as_i64()?,
        label + ".selectionRange.end.line") and ok
      ok = h.assert_eq[I64](
        exp.sel._4,
        item_nav("selectionRange")("end")("character").as_i64()?,
        label + ".selectionRange.end.character") and ok
      ok = h.assert_eq[I64](
        exp.rng._1,
        item_nav("range")("start")("line").as_i64()?,
        label + ".range.start.line") and ok
      ok = h.assert_eq[I64](
        exp.rng._2,
        item_nav("range")("start")("character").as_i64()?,
        label + ".range.start.character") and ok
      ok = h.assert_eq[I64](
        exp.rng._3,
        item_nav("range")("end")("line").as_i64()?,
        label + ".range.end.line") and ok
      ok = h.assert_eq[I64](
        exp.rng._4,
        item_nav("range")("end")("character").as_i64()?,
        label + ".range.end.character") and ok
      ok
    else
      h.fail(
        label + " missing or has wrong structure (expected: "
          + exp.name + ")")
      false
    end

primitive _CHierFindByPos
  """
  Find the index in `nav` whose `item_key.uri` and
  `item_key.selectionRange.start` match `exp`. Returns None if not found.
  Using (uri, selectionRange.start) as identity rather than name avoids false
  matches when two entries share a name (e.g. same-named methods in different
  classes).
  """
  fun apply(
    nav: JsonNav, item_key: String, exp: _CHierItem val): (USize | None) =>
    try
      let n = nav.size()?
      var i: USize = 0
      while i < n do
        try
          let entry = nav(i)(item_key)
          let sel = entry("selectionRange")("start")
          if
            (entry("uri").as_string()? == exp.uri) and
            (sel("line").as_i64()? == exp.sel._1) and
            (sel("character").as_i64()? == exp.sel._2)
          then
            return i
          end
        end
        i = i + 1
      end
    end
    None

primitive _CHierCheckEntry
  """
  Find an entry in the call hierarchy result array by name, check its item
  fields and fromRanges. `item_key` is "from" for incoming, "to" for outgoing.
  """
  fun apply(
    nav: JsonNav,
    item_key: String,
    exp: _CHierExpected val,
    h: TestHelper): Bool
  =>
    let i =
      match \exhaustive\ _CHierFindByPos(nav, item_key, exp.item)
      | let idx: USize => idx
      | None =>
        h.fail(item_key + " entry '" + exp.item.name + "' not found")
        return false
      end
    let label: String val = item_key + "['" + exp.item.name + "']"
    var ok = _CHierCheckItem(nav(i)(item_key), exp.item, label, h)
    try
      let ranges_nav = nav(i)("fromRanges")
      let expected_count = exp.from_ranges.size()
      ok = h.assert_eq[USize](
        expected_count,
        try ranges_nav.size()? else 0 end,
        label + " fromRanges count") and ok
      for (ri, rng) in exp.from_ranges.pairs() do
        let rl: String val = label + " fromRanges[" + ri.string() + "]"
        ok = h.assert_eq[I64](
          rng._1,
          ranges_nav(ri)("start")("line").as_i64()?,
          rl + ".start.line") and ok
        ok = h.assert_eq[I64](
          rng._2,
          ranges_nav(ri)("start")("character").as_i64()?,
          rl + ".start.character") and ok
        ok = h.assert_eq[I64](
          rng._3,
          ranges_nav(ri)("end")("line").as_i64()?,
          rl + ".end.line") and ok
        ok = h.assert_eq[I64](
          rng._4,
          ranges_nav(ri)("end")("character").as_i64()?,
          rl + ".end.character") and ok
      end
    else
      h.fail(label + " fromRanges missing or malformed")
      ok = false
    end
    ok

class val _PrepareCHierChecker
  """
  Checker for textDocument/prepareCallHierarchy.
  Pass None for expected to assert a null result (non-method cursor).
  """
  let _pos: (I64, I64)
  let _expected: (_CHierItem val | None)

  new val create(pos': (I64, I64), expected: (_CHierItem val | None)) =>
    _pos = pos'
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().prepare_call_hierarchy()

  fun lsp_params(): (None | JsonObject) =>
    JsonObject.update(
      "position",
      JsonObject
        .update("line", _pos._1)
        .update("character", _pos._2))

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match \exhaustive\ _expected
    | None =>
      match res.result
      | None => true
      else
        h.fail("prepare: expected null for non-method cursor, got non-null")
        false
      end
    | let exp: _CHierItem val =>
      match res.result
      | let arr: JsonArray =>
        let nav = JsonNav(arr)
        var ok =
          h.assert_eq[USize](
            1,
            try nav.size()? else 0 end,
            "prepare: expected single item")
        ok = _CHierCheckItem(nav(0), exp, "item[0]", h) and ok
        ok
      | None =>
        h.fail("prepare: expected item, got null")
        false
      else
        h.fail("prepare: expected array result")
        false
      end
    end

class val _CHierCallsChecker
  """
  Checker for callHierarchy/incomingCalls and callHierarchy/outgoingCalls.
  `item_key` is "from" for incoming, "to" for outgoing.
  """
  let _method: String
  let _item_key: String
  let _item_uri: String val
  let _sel_pos: (I64, I64)
  let _expected: Array[_CHierExpected val] val

  new val create(
    method': String,
    item_key': String,
    item_uri': String,
    sel_pos': (I64, I64),
    expected: Array[_CHierExpected val] val)
  =>
    _method = method'
    _item_key = item_key'
    _item_uri = item_uri'
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
          _method + ": wrong entry count")
      for exp in _expected.values() do
        ok = _CHierCheckEntry(nav, _item_key, exp, h) and ok
      end
      ok
    else
      h.fail(_method + ": expected array result")
      false
    end

class \nodoc\ iso _PrepareCallHierarchyOnMethodTest is UnitTest
  """
  Cursor on f_a name (line 1, col 6) — expects a one-element result
  with all CallHierarchyItem fields verified.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/prepare_method"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    _RunLspChecks(
      h,
      _server,
      t_call_a,
      [ _PrepareCHierChecker(
          (1, 6),
          _CHierItem(
            "f_a",
            6,
            "_TCallA",
            workspace_dir,
            t_call_a,
            (1, 6, 1, 9),
            (1, 2, 1, 27))) ])

class \nodoc\ iso _PrepareCallHierarchyOnNonMethodTest is UnitTest
  """
  Cursor on class name _TCallA (line 0, col 6) — not a method.
  Expects null result.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/prepare_non_method"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "call_hierarchy/_t_call_a.pony",
      [_PrepareCHierChecker((0, 6), None)])

class \nodoc\ iso _IncomingCallsForFaTest is UnitTest
  """
  Incoming calls for f_a — f_b, f_c, f_lambda call f_a once each; f_d calls
  it twice; act calls it once; _TConstructorWithBody.create calls it once. The
  f_d entry verifies that two call sites from the same caller are grouped into
  one entry with two fromRanges (not two separate entries). f_lambda verifies
  that a caller in a different file with a nested object literal is correctly
  attributed. The create entry verifies that a `new` constructor is correctly
  identified as a caller (tk_newref call sites collected as incoming calls).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/incoming_calls"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let t_call_b = "call_hierarchy/_t_call_b.pony"
    let t_call_c = "call_hierarchy/_t_call_c.pony"
    let t_call_d = "call_hierarchy/_t_call_d.pony"
    let t_call_actor = "call_hierarchy/_t_call_actor.pony"
    let t_call_lambda = "call_hierarchy/_t_call_lambda.pony"
    let t_call_constructor = "call_hierarchy/_t_call_constructor.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_call_a))
    _RunLspChecks(
      h,
      _server,
      t_call_a,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().incoming_calls(),
          "from",
          item_uri,
          (1, 6),
          [ _CHierExpected(
              _CHierItem(
                "f_b",
                6,
                "_TCallB",
                workspace_dir,
                t_call_b,
                (1, 6, 1, 9),
                (1, 2, 2, 10)),
              [(2, 6, 2, 9)])
            _CHierExpected(
              _CHierItem(
                "f_c",
                6,
                "_TCallC",
                workspace_dir,
                t_call_c,
                (1, 6, 1, 9),
                (1, 2, 3, 11)),
              [(2, 6, 2, 9)])
            _CHierExpected(
              _CHierItem(
                "f_d",
                6,
                "_TCallD",
                workspace_dir,
                t_call_d,
                (1, 6, 1, 9),
                (1, 2, 3, 10)),
              [(2, 6, 2, 9); (3, 6, 3, 9)])
            _CHierExpected(
              _CHierItem(
                "act",
                6,
                "_TCallActor",
                workspace_dir,
                t_call_actor,
                (4, 5, 4, 8),
                (4, 2, 5, 10)),
              [(5, 6, 5, 9)])
            _CHierExpected(
              _CHierItem(
                "f_lambda",
                6,
                "_TCallLambda",
                workspace_dir,
                t_call_lambda,
                (1, 6, 1, 14),
                (1, 2, 9, 10)),
              [(2, 6, 2, 9); (8, 6, 8, 9)])
            _CHierExpected(
              _CHierItem(
                "create",
                9,
                "_TConstructorWithBody",
                workspace_dir,
                t_call_constructor,
                (9, 6, 9, 12),
                (9, 2, 10, 10)),
              [(10, 6, 10, 9)]) ]) ])

class \nodoc\ iso _IncomingCallsEmptyTest is UnitTest
  """
  Incoming calls for f_c — f_c is never called in the fixture.
  Expects empty array (not null).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/incoming_calls_empty"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_c = "call_hierarchy/_t_call_c.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_call_c))
    _RunLspChecks(
      h,
      _server,
      t_call_c,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().incoming_calls(),
          "from",
          item_uri,
          (1, 6),
          []) ])

class \nodoc\ iso _OutgoingCallsEmptyTest is UnitTest
  """
  Outgoing calls for f_a — f_a's body is `None` (a type literal). Expects
  empty array (not null). Also exercises the synthetic tk_newref filter:
  without it, the desugared None.create would appear as a spurious entry.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/outgoing_calls_empty"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_call_a))
    _RunLspChecks(
      h,
      _server,
      t_call_a,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (1, 6),
          []) ])

class \nodoc\ iso _OutgoingCallsForFbTest is UnitTest
  """
  Outgoing calls for f_b — calls f_a once. Expects one entry.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/outgoing_calls_single"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let t_call_b = "call_hierarchy/_t_call_b.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_call_b))
    _RunLspChecks(
      h,
      _server,
      t_call_b,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (1, 6),
          [ _CHierExpected(
              _CHierItem(
                "f_a",
                6,
                "_TCallA",
                workspace_dir,
                t_call_a,
                (1, 6, 1, 9),
                (1, 2, 1, 27)),
              [(2, 6, 2, 9)]) ]) ])

class \nodoc\ iso _OutgoingCallsForFcTest is UnitTest
  """
  Outgoing calls for f_c — calls f_a and f_b once each. Expects two entries.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/outgoing_calls_multiple"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let t_call_b = "call_hierarchy/_t_call_b.pony"
    let t_call_c = "call_hierarchy/_t_call_c.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_call_c))
    _RunLspChecks(
      h,
      _server,
      t_call_c,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (1, 6),
          [ _CHierExpected(
              _CHierItem(
                "f_a",
                6,
                "_TCallA",
                workspace_dir,
                t_call_a,
                (1, 6, 1, 9),
                (1, 2, 1, 27)),
              [(2, 6, 2, 9)])
            _CHierExpected(
              _CHierItem(
                "f_b",
                6,
                "_TCallB",
                workspace_dir,
                t_call_b,
                (1, 6, 1, 9),
                (1, 2, 2, 10)),
              [(3, 6, 3, 9)]) ]) ])

class \nodoc\ iso _OutgoingCallsForFdTest is UnitTest
  """
  Outgoing calls for f_d — calls f_a twice from separate sites. Expects one
  entry for f_a with two fromRanges, verifying the multi-call-site grouping
  path in _OutgoingCallCollector.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/outgoing_calls_multi_site"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let t_call_d = "call_hierarchy/_t_call_d.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_call_d))
    _RunLspChecks(
      h,
      _server,
      t_call_d,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (1, 6),
          [ _CHierExpected(
              _CHierItem(
                "f_a",
                6,
                "_TCallA",
                workspace_dir,
                t_call_a,
                (1, 6, 1, 9),
                (1, 2, 1, 27)),
              [(2, 6, 2, 9); (3, 6, 3, 9)]) ]) ])

class val _PrepareKindChecker
  """
  Prepare checker that asserts kind, name, and detail fields.
  Used when the range assertions are secondary to testing kind values
  (e.g. constructors returning kind=9, behaviors returning kind=6).
  """
  let _pos: (I64, I64)
  let _expected_kind: I64
  let _expected_name: String
  let _expected_detail: String

  new val create(
    pos': (I64, I64),
    expected_kind: I64,
    expected_name: String,
    expected_detail: String)
  =>
    _pos = pos'
    _expected_kind = expected_kind
    _expected_name = expected_name
    _expected_detail = expected_detail

  fun lsp_method(): String =>
    Methods.text_document().prepare_call_hierarchy()

  fun lsp_params(): (None | JsonObject) =>
    JsonObject.update(
      "position",
      JsonObject
        .update("line", _pos._1)
        .update("character", _pos._2))

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      try
        let item = JsonNav(arr)(0)
        let ok_kind =
          h.assert_eq[I64](
            _expected_kind, item("kind").as_i64()?, "prepare: kind")
        let ok_name =
          h.assert_eq[String](
            _expected_name, item("name").as_string()?, "prepare: name")
        let ok_detail =
          h.assert_eq[String](
            _expected_detail, item("detail").as_string()?, "prepare: detail")
        ok_kind and ok_name and ok_detail
      else
        h.fail("prepare: missing fields in result")
        false
      end
    | None =>
      h.fail("prepare: expected array, got null")
      false
    else
      h.fail("prepare: expected array result")
      false
    end

class \nodoc\ iso _PrepareConstructorKindTest is UnitTest
  """
  Cursor on `new create` — expects kind=9 (constructor).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/prepare_constructor_kind"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "call_hierarchy/_t_call_actor.pony",
      [_PrepareKindChecker((1, 6), 9, "create", "_TCallActor")])

class \nodoc\ iso _PrepareBeKindTest is UnitTest
  """
  Cursor on `be act` — expects kind=6 (method).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/prepare_be_kind"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "call_hierarchy/_t_call_actor.pony",
      [_PrepareKindChecker((4, 5), 6, "act", "_TCallActor")])

class \nodoc\ iso _OutgoingCallsForActTest is UnitTest
  """
  Outgoing calls for `act` — calls f_a once. Exercises the tk_be code path.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/outgoing_calls_be"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let t_call_actor = "call_hierarchy/_t_call_actor.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_call_actor))
    _RunLspChecks(
      h,
      _server,
      t_call_actor,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (4, 5),
          [ _CHierExpected(
              _CHierItem(
                "f_a",
                6,
                "_TCallA",
                workspace_dir,
                t_call_a,
                (1, 6, 1, 9),
                (1, 2, 1, 27)),
              [(5, 6, 5, 9)]) ]) ])

class \nodoc\ iso _PrepareFromCallRefTest is UnitTest
  """
  Cursor on the `f_a` call site in f_b's body (line 2, col 6 in
  _t_call_b.pony) — expects prepare to resolve to f_a in _t_call_a.pony.
  Exercises Case 3 of _method_for_node (cursor on a call reference).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "call_hierarchy/integration/prepare_from_call_ref"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let t_call_b = "call_hierarchy/_t_call_b.pony"
    _RunLspChecks(
      h,
      _server,
      t_call_b,
      [ _PrepareCHierChecker(
          (2, 6),
          _CHierItem(
            "f_a",
            6,
            "_TCallA",
            workspace_dir,
            t_call_a,
            (1, 6, 1, 9),
            (1, 2, 1, 27))) ])

class \nodoc\ iso _OutgoingCallsNoLambdaCallsTest is UnitTest
  """
  Outgoing calls for f_lambda — the method body calls f_a twice (before and
  after an object literal containing g) and g has no calls of its own.
  Both outer calls to f_a must appear; no calls from inside g must appear.

  Regression test for the _OutgoingCallCollector nested-method fix: without
  the _nested_depth / leave() guard, returning Stop on a nested method node
  aborts the entire traversal — so the second a.f_a() (after the object
  literal) is silently dropped. With the fix both call sites are recorded and
  calls inside g are excluded.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "call_hierarchy/integration/outgoing_calls_no_lambda_calls"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let t_call_lambda = "call_hierarchy/_t_call_lambda.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_call_lambda))
    _RunLspChecks(
      h,
      _server,
      t_call_lambda,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (1, 6),
          [ _CHierExpected(
              _CHierItem(
                "f_a",
                6,
                "_TCallA",
                workspace_dir,
                t_call_a,
                (1, 6, 1, 9),
                (1, 2, 1, 27)),
              [(2, 6, 2, 9); (8, 6, 8, 9)]) ]) ])

class \nodoc\ iso _IncomingCallsPolyATest is UnitTest
  """
  Incoming calls for _TPolyA.poly_method — only call_a calls it directly;
  call_b calls _TPolyB.poly_method instead.

  Regression test for H1: verifies that incomingCalls routed via an item
  pointing to _TPolyA.poly_method (selectionRange in _t_poly.pony at line 1)
  correctly resolves to _TPolyA.poly_method and returns only its callers,
  not _TPolyB.poly_method's callers. Both classes live in the same file so
  only the position distinguishes them — the strongest possible disambiguation.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "call_hierarchy/integration/incoming_calls_poly_a"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_poly = "call_hierarchy/_t_poly.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_poly))
    _RunLspChecks(
      h,
      _server,
      t_poly,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().incoming_calls(),
          "from",
          item_uri,
          (1, 6),
          [ _CHierExpected(
              _CHierItem(
                "call_a",
                6,
                "_TPolyUser",
                workspace_dir,
                t_poly,
                (7, 6, 7, 12),
                (7, 2, 8, 18)),
              [(8, 6, 8, 17)]) ]) ])

class \nodoc\ iso _IncomingCallsPolyBTest is UnitTest
  """
  Incoming calls for _TPolyB.poly_method — only call_b calls it directly;
  call_a calls _TPolyA.poly_method instead.

  Counterpart to _IncomingCallsPolyATest: item points to line 4 in the same
  file. Result must be {call_b}, not {call_a}. Together the two tests prove
  that position-based routing correctly distinguishes between two same-named
  methods in the same file.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "call_hierarchy/integration/incoming_calls_poly_b"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_poly = "call_hierarchy/_t_poly.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_poly))
    _RunLspChecks(
      h,
      _server,
      t_poly,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().incoming_calls(),
          "from",
          item_uri,
          (4, 6),
          [ _CHierExpected(
              _CHierItem(
                "call_b",
                6,
                "_TPolyUser",
                workspace_dir,
                t_poly,
                (10, 6, 10, 12),
                (10, 2, 11, 18)),
              [(11, 6, 11, 17)]) ]) ])

class \nodoc\ iso _OutgoingCallsCrossPackageTest is UnitTest
  """
  Outgoing calls for _TCrossCaller.cross_call, where the callee
  (_TCallee.callee_method) lives in the call_hierarchy/callee sub-package.
  Exercises the cross-package definition resolution in _OutgoingCallCollector:
  `definitions()` on `c.callee_method()` resolves to a method in a package
  different from the one containing cross_call.
  `_t_cross_caller.pony` uses "callee", so the sub-package is compiled as a
  dependency in the same ponyc run — no separate compilation or timing concern.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "call_hierarchy/integration/outgoing_calls_cross_package"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_cross_caller = "call_hierarchy/_t_cross_caller.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_cross_caller))
    _RunLspChecks(
      h,
      _server,
      t_cross_caller,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (3, 6),
          [ _CHierExpected(
              _CHierItem(
                "callee_method",
                6,
                "TCallee",
                workspace_dir,
                "call_hierarchy/callee/t_callee.pony",
                (4, 6, 4, 19),
                (4, 2, 4, 37)),
              [(4, 6, 4, 19)]) ]) ])

class \nodoc\ iso _OutgoingCallsForFNewExplicitTest is UnitTest
  """
  Outgoing calls for _TCallConstructorCaller.f_new_explicit, which calls
  _TConstructed.create() using the explicit constructor syntax. Expects one
  entry for `create` in `_TConstructed` — verifies that _is_synthetic_newref
  does NOT filter explicit constructor calls (Foo.create() form).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "call_hierarchy/integration/outgoing_calls_explicit_constructor"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_ctor = "call_hierarchy/_t_call_constructor.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_ctor))
    _RunLspChecks(
      h,
      _server,
      t_ctor,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (4, 6),
          [ _CHierExpected(
              _CHierItem(
                "create",
                9,
                "_TConstructed",
                workspace_dir,
                t_ctor,
                (1, 6, 1, 12),
                (1, 2, 1, 24)),
              [(5, 18, 5, 24)]) ]) ])

class \nodoc\ iso _OutgoingCallsFromConstructorTest is UnitTest
  """
  Outgoing calls for _TConstructorWithBody.create — a constructor whose body
  calls a.f_a(). Expects one entry for f_a in _TCallA, verifying that
  _OutgoingCallCollector works when the source method is a `new` (not just
  fun or be).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "call_hierarchy/integration/outgoing_calls_from_constructor"

  fun apply(h: TestHelper) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let t_ctor = "call_hierarchy/_t_call_constructor.pony"
    let t_call_a = "call_hierarchy/_t_call_a.pony"
    let item_uri = Uris.from_path(Path.join(workspace_dir, t_ctor))
    _RunLspChecks(
      h,
      _server,
      t_ctor,
      [ _CHierCallsChecker(
          Methods.call_hierarchy().outgoing_calls(),
          "to",
          item_uri,
          (9, 6),
          [ _CHierExpected(
              _CHierItem(
                "f_a",
                6,
                "_TCallA",
                workspace_dir,
                t_call_a,
                (1, 6, 1, 9),
                (1, 2, 1, 27)),
              [(10, 6, 10, 9)]) ]) ])
