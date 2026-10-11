use "pony_test"
use "files"
use "pony_compiler"
use ".."

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_InspectTypeTest)
    test(_InspectDefinitionTest)
    test(_InspectEnclosingTest)
    test(_InspectRecoverTest)
    test(_InspectSendableTest)
    test(_InspectErrorTest)
    test(_ScopeBasicTest)
    test(_ScopeRecoverTest)
    test(_ScopeErrorTest)
    test(_ExportsBasicTest)
    test(_ExportsPrivateFilterTest)
    test(_ExportsDocstringTest)
    test(_ExportsBuiltinTest)
    test(_ExportsErrorTest)
    test(_CapSubtypingBasicTest)
    test(_CapSubtypingEphemeralTest)
    test(_CapSubtypingInvalidTest)
    test(_MethodEnumeratorOwnTest)
    test(_MethodEnumeratorInheritedTest)
    test(_MethodEnumeratorParamsTest)
    test(_MethodEnumeratorCallableAtTest)
    test(_TypeApiFromTypeTest)
    test(_TypeApiFromPositionTest)
    test(_TypeApiConstructorsTest)
    test(_TypeApiTraitsTest)
    test(_TypeApiUnionTest)
    test(_TypeApiUnionDivergentTest)
    test(_TypeApiViewpointTest)
    test(_TypeApiEphemeralTest)
    test(_TypeApiGenericTest)
    test(_TypeApiErrorTest)
    test(_CheckCompatibleTest)
    test(_CheckIncompatibleTest)
    test(_CheckCapMismatchTest)
    test(_CheckSuggestionsTest)
    test(_CheckErrorTest)
    test(_ImplementorsNominalTest)
    test(_ImplementorsStructuralTest)
    test(_ImplementorsIntersectionProvidesTest)
    test(_ImplementorsPrivateFilterTest)
    test(_ImplementorsErrorTest)

class \nodoc\ iso _InspectTypeTest is UnitTest
  fun name(): String => "inspect/type"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // collector at line 70: collector.add("one")
    match InspectQuery(session, fixture_file, 70, 5)
    | let r: InspectResult val =>
      h.assert_eq[String val]("Collector ref", r.type_string)
    | let err: String val =>
      h.fail("inspect at 70:5 failed: " + err)
    end

    // worker at line 74: let worker = Worker("w1")
    match InspectQuery(session, fixture_file, 74, 5)
    | let r: InspectResult val =>
      h.assert_eq[String val]("Worker tag", r.type_string)
    | let err: String val =>
      h.fail("inspect at 74:5 failed: " + err)
    end

    // box_str at line 77: let box_str = GenericBox[String]("hello")
    match InspectQuery(session, fixture_file, 77, 5)
    | let r: InspectResult val =>
      h.assert_eq[String val](
        "GenericBox[String val] ref", r.type_string)
    | let err: String val =>
      h.fail("inspect at 77:5 failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _InspectDefinitionTest is UnitTest
  fun name(): String => "inspect/definition"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    match InspectQuery(session, fixture_file, 70, 5)
    | let r: InspectResult val =>
      match r.definition_kind
      | let k: String val =>
        h.assert_eq[String val]("TK_LET", k)
      | None =>
        h.fail("expected definition_kind 'tk_let', got None")
      end
    | let err: String val =>
      h.fail("inspect at 70:5 failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _InspectEnclosingTest is UnitTest
  fun name(): String => "inspect/enclosing"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // _items.push(item) inside Collector.add at line 16
    match InspectQuery(session, fixture_file, 16, 5)
    | let r: InspectResult val =>
      match r.enclosing_method
      | let m: String val =>
        h.assert_eq[String val]("add", m)
      | None =>
        h.fail("expected enclosing method 'add', got None")
      end

      match r.enclosing_type
      | let t: String val =>
        h.assert_eq[String val]("Collector", t)
      | None =>
        h.fail("expected enclosing type 'Collector', got None")
      end

      match r.enclosing_receiver_cap
      | let c: String val =>
        h.assert_eq[String val]("ref", c)
      | None =>
        h.fail("expected enclosing receiver cap 'ref', got None")
      end
    | let err: String val =>
      h.fail("inspect at 16:5 failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _InspectRecoverTest is UnitTest
  fun name(): String => "inspect/recover"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // Inside Helpers.make_val_string recover block, line 36
    match InspectQuery(session, fixture_file, 36, 7)
    | let r: InspectResult val =>
      h.assert_true(
        r.in_recover,
        "expected in_recover=true inside recover block")
    | let err: String val =>
      h.fail("inspect at 36:7 failed: " + err)
    end

    // Outside recover, line 70 (Main.create body)
    match InspectQuery(session, fixture_file, 70, 5)
    | let r: InspectResult val =>
      h.assert_false(
        r.in_recover,
        "expected in_recover=false outside recover block")
    | let err: String val =>
      h.fail("inspect at 70:5 failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _InspectSendableTest is UnitTest
  fun name(): String => "inspect/sendable"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // helpers_result = Helpers.make_val_string() — String val, sendable
    match InspectQuery(session, fixture_file, 79, 5)
    | let r: InspectResult val =>
      h.assert_true(
        r.sendable,
        "expected sendable=true for val type, got type: " + r.type_string)
    | let err: String val =>
      h.fail("inspect at 79:5 failed: " + err)
    end

    // collector is Collector ref — not sendable
    match InspectQuery(session, fixture_file, 70, 5)
    | let r: InspectResult val =>
      h.assert_false(
        r.sendable,
        "expected sendable=false for ref type, got type: " + r.type_string)
    | let err: String val =>
      h.fail("inspect at 70:5 failed: " + err)
    end

    // worker is Worker tag — sendable
    match InspectQuery(session, fixture_file, 74, 5)
    | let r: InspectResult val =>
      h.assert_true(
        r.sendable,
        "expected sendable=true for tag type, got type: " + r.type_string)
    | let err: String val =>
      h.fail("inspect at 74:5 failed: " + err)
    end

    // iso_string is String iso — sendable
    match InspectQuery(session, fixture_file, 80, 5)
    | let r: InspectResult val =>
      h.assert_true(
        r.sendable,
        "expected sendable=true for iso type, got type: " + r.type_string)
    | let err: String val =>
      h.fail("inspect at 80:5 failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _InspectErrorTest is UnitTest
  fun name(): String => "inspect/error"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    match InspectQuery(session, "/nonexistent.pony", 1, 1)
    | let _: InspectResult val =>
      h.fail("expected error for nonexistent file, got result")
    | let err: String val =>
      h.assert_true(
        err.contains("not found"),
        "error should mention 'not found': " + err)
    end

    let fixture_file = _FixtureHelper.fixture_path(h)
    match InspectQuery(session, fixture_file, 9999, 1)
    | let _: InspectResult val =>
      h.fail("expected error for invalid position, got result")
    | let err: String val =>
      h.assert_true(
        err.contains("no AST node"),
        "error should mention 'no AST node': " + err)
    end

    session.dispose()

class \nodoc\ iso _ScopeBasicTest is UnitTest
  fun name(): String => "scope/basic"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // collector.add("one") at line 70 inside Main.create
    match ScopeQuery(session, fixture_file, 70, 5)
    | let result: ScopeResult val =>
      h.assert_false(
        result.in_recover,
        "expected in_recover=false in Main.create")

      match _FixtureHelper.find_scope_entry(result, "collector")
      | let e: ScopeEntry val =>
        h.assert_eq[String val]("local", e.kind)
        h.assert_eq[String val]("Collector ref", e.type_string)
        h.assert_true(e.accessible, "collector should be accessible")
      | None =>
        h.fail("expected 'collector' in scope")
      end

      match _FixtureHelper.find_scope_entry(result, "env")
      | let e: ScopeEntry val =>
        h.assert_eq[String val]("parameter", e.kind)
        h.assert_eq[String val]("Env val", e.type_string)
        h.assert_true(e.accessible, "env should be accessible")
      | None =>
        h.fail("expected 'env' in scope")
      end

      match _FixtureHelper.find_scope_entry(result, "create")
      | let e: ScopeEntry val =>
        h.assert_eq[String val]("method", e.kind)
      | None =>
        h.fail("expected 'create' in scope")
      end

      match _FixtureHelper.find_scope_entry(result, "Collector")
      | let e: ScopeEntry val =>
        h.assert_eq[String val]("type", e.kind)
      | None =>
        h.fail("expected 'Collector' in scope")
      end
    | let err: String val =>
      h.fail("scope at 70:5 failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ScopeRecoverTest is UnitTest
  fun name(): String => "scope/recover"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // inner_local.string() at line 164 inside ScopeFixture.scope_test
    // recover block
    match ScopeQuery(session, fixture_file, 164, 7)
    | let result: ScopeResult val =>
      h.assert_true(
        result.in_recover,
        "expected in_recover=true inside recover block")

      // inner_local is defined inside the recover — accessible
      match _FixtureHelper.find_scope_entry(result, "inner_local")
      | let e: ScopeEntry val =>
        h.assert_eq[String val]("local", e.kind)
        h.assert_true(
          e.accessible,
          "inner_local should be accessible inside recover")
      | None =>
        h.fail("expected 'inner_local' in scope")
      end

      // outer_val is String val — sendable, so accessible
      match _FixtureHelper.find_scope_entry(result, "outer_val")
      | let e: ScopeEntry val =>
        h.assert_eq[String val]("local", e.kind)
        h.assert_eq[String val]("String val", e.type_string)
        h.assert_true(
          e.accessible,
          "outer_val (val) should be accessible in recover")
      | None =>
        h.fail("expected 'outer_val' in scope")
      end

      // outer_ref is String ref — not sendable, so inaccessible
      match _FixtureHelper.find_scope_entry(result, "outer_ref")
      | let e: ScopeEntry val =>
        h.assert_eq[String val]("local", e.kind)
        h.assert_eq[String val]("String ref", e.type_string)
        h.assert_false(
          e.accessible,
          "outer_ref (ref) should not be accessible in recover")
        match e.reason
        | let r: String val =>
          h.assert_true(
            r.contains("ref not accessible"),
            "reason should explain inaccessibility: " + r)
        | None =>
          h.fail("expected a reason for inaccessible outer_ref")
        end
      | None =>
        h.fail("expected 'outer_ref' in scope")
      end

      // _val_field is a field — inaccessible because this is tag
      match _FixtureHelper.find_scope_entry(result, "_val_field")
      | let e: ScopeEntry val =>
        h.assert_eq[String val]("field", e.kind)
        h.assert_false(
          e.accessible,
          "_val_field should not be accessible in recover")
        match e.reason
        | let r: String val =>
          h.assert_true(
            r.contains("tag"),
            "field reason should mention tag: " + r)
        | None =>
          h.fail("expected a reason for inaccessible _val_field")
        end
      | None =>
        h.fail("expected '_val_field' in scope")
      end
    | let err: String val =>
      h.fail("scope at 164:7 failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ScopeErrorTest is UnitTest
  fun name(): String => "scope/error"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    match ScopeQuery(session, "/nonexistent.pony", 1, 1)
    | let _: ScopeResult val =>
      h.fail("expected error for nonexistent file, got result")
    | let err: String val =>
      h.assert_true(
        err.contains("not found"),
        "error should mention 'not found': " + err)
    end

    let fixture_file = _FixtureHelper.fixture_path(h)
    match ScopeQuery(session, fixture_file, 9999, 1)
    | let _: ScopeResult val =>
      h.fail("expected error for invalid position, got result")
    | let err: String val =>
      h.assert_true(
        err.contains("no AST node"),
        "error should mention 'no AST node': " + err)
    end

    session.dispose()

class \nodoc\ iso _ExportsBasicTest is UnitTest
  fun name(): String => "exports/basic"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    let fixture_dir =
      Path.dir(Path.join(Path.dir(__loc.file()), "fixture/main.pony"))
    match ExportsQuery(session, fixture_dir)
    | let result: ExportsResult val =>
      h.assert_true(
        result.entries.size() > 0,
        "expected at least one export")
      h.assert_true(
        result.package_path.size() > 0,
        "expected non-empty package_path")

      match _FixtureHelper.find_export(result, "Collector")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("class", e.kind)
      | None =>
        h.fail("expected 'Collector' in exports")
      end

      match _FixtureHelper.find_export(result, "Worker")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("actor", e.kind)
      | None =>
        h.fail("expected 'Worker' in exports")
      end

      match _FixtureHelper.find_export(result, "Helpers")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("primitive", e.kind)
      | None =>
        h.fail("expected 'Helpers' in exports")
      end

      match _FixtureHelper.find_export(result, "Describable")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("trait", e.kind)
      | None =>
        h.fail("expected 'Describable' in exports")
      end

      match _FixtureHelper.find_export(result, "Countable")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("interface", e.kind)
      | None =>
        h.fail("expected 'Countable' in exports")
      end

      match _FixtureHelper.find_export(result, "StringOrNone")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("type_alias", e.kind)
      | None =>
        h.fail("expected 'StringOrNone' in exports")
      end

      match _FixtureHelper.find_export(result, "ExportStruct")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("struct", e.kind)
      | None =>
        h.fail("expected 'ExportStruct' in exports")
      end

      // Verify sorted order
      var prev_name: String val = ""
      for entry in result.entries.values() do
        h.assert_true(
          entry.name >= prev_name,
          "entries not sorted: '" + prev_name +
            "' before '" + entry.name + "'")
        prev_name = entry.name
      end
    | let err: String val =>
      h.fail("exports query failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ExportsPrivateFilterTest is UnitTest
  fun name(): String => "exports/private_filter"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    let fixture_dir =
      Path.dir(Path.join(Path.dir(__loc.file()), "fixture/main.pony"))
    match ExportsQuery(session, fixture_dir)
    | let result: ExportsResult val =>
      match _FixtureHelper.find_export(result, "_PrivateHelper")
      | let _: ExportEntry val =>
        h.fail("private type '_PrivateHelper' should not appear in exports")
      | None =>
        h.assert_true(true)
      end
    | let err: String val =>
      h.fail("exports query failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ExportsDocstringTest is UnitTest
  fun name(): String => "exports/docstring"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    let fixture_dir =
      Path.dir(Path.join(Path.dir(__loc.file()), "fixture/main.pony"))
    match ExportsQuery(session, fixture_dir)
    | let result: ExportsResult val =>
      match _FixtureHelper.find_export(result, "DocumentedType")
      | let e: ExportEntry val =>
        match e.docstring
        | let d: String val =>
          h.assert_true(
            d.contains("docstring for testing"),
            "docstring should contain expected text: " + d)
        | None =>
          h.fail("expected docstring for DocumentedType")
        end
      | None =>
        h.fail("expected 'DocumentedType' in exports")
      end

      // Types without docstrings should have None
      match _FixtureHelper.find_export(result, "Collector")
      | let e: ExportEntry val =>
        match e.docstring
        | let _: String val =>
          h.fail("Collector should have no docstring")
        | None =>
          h.assert_true(true)
        end
      | None =>
        h.fail("expected 'Collector' in exports")
      end
    | let err: String val =>
      h.fail("exports query failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ExportsBuiltinTest is UnitTest
  fun name(): String => "exports/builtin"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    match ExportsQuery(session, "builtin")
    | let result: ExportsResult val =>
      h.assert_true(
        result.entries.size() > 0,
        "builtin should have exports")

      match _FixtureHelper.find_export(result, "String")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("class", e.kind)
      | None =>
        h.fail("expected 'String' in builtin exports")
      end

      match _FixtureHelper.find_export(result, "None")
      | let e: ExportEntry val =>
        h.assert_eq[String val]("primitive", e.kind)
      | None =>
        h.fail("expected 'None' in builtin exports")
      end
    | let err: String val =>
      h.fail("exports for 'builtin' failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ExportsErrorTest is UnitTest
  fun name(): String => "exports/error"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    match ExportsQuery(session, "nonexistent_package_xyz")
    | let _: ExportsResult val =>
      h.fail("expected error for nonexistent package, got result")
    | let err: String val =>
      h.assert_true(
        err.contains("not found"),
        "error should mention 'not found': " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiFromTypeTest is UnitTest
  fun name(): String => "type_api/from_type"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // CapMethods at val: box/val/tag callable, ref not callable
    match TypeAPIQuery.from_type(session, "CapMethods", "val")
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("class", result.type_kind)
      h.assert_eq[String val]("CapMethods", result.type_name)

      // read_only (box), immutable_op (val), identity (tag),
      // partial_lookup (box), with_default (box) = 5 callable
      h.assert_eq[USize](5, result.callable.size())
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "read_only"),
        "read_only (box) should be callable at val")
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "immutable_op"),
        "immutable_op (val) should be callable at val")
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "identity"),
        "identity (tag) should be callable at val")

      // mutate (ref) not callable
      h.assert_eq[USize](1, result.not_callable.size())
      h.assert_true(
        _FixtureHelper.has_not_callable(result.not_callable, "mutate"),
        "mutate (ref) should not be callable at val")
    | let err: String val =>
      h.fail("type_api from_type failed: " + err)
    end

    // CapMethods at ref: ref/box/tag callable, val not callable
    match TypeAPIQuery.from_type(session, "CapMethods", "ref")
    | let result: TypeAPIResult val =>
      h.assert_eq[USize](5, result.callable.size())
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "mutate"),
        "mutate (ref) should be callable at ref")
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "read_only"),
        "read_only (box) should be callable at ref")

      h.assert_eq[USize](1, result.not_callable.size())
      h.assert_true(
        _FixtureHelper.has_not_callable(result.not_callable, "immutable_op"),
        "immutable_op (val) should not be callable at ref")
    | let err: String val =>
      h.fail("type_api from_type at ref failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiFromPositionTest is UnitTest
  fun name(): String => "type_api/from_position"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // collector at line 70 is Collector ref
    match TypeAPIQuery.from_position(session, fixture_file, 70, 5)
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("class", result.type_kind)
      h.assert_true(
        result.type_name.contains("Collector"),
        "type_name should contain Collector: " + result.type_name)

      // At ref: add (ref), size (box), describe (box) all callable
      h.assert_eq[USize](3, result.callable.size())
      h.assert_eq[USize](0, result.not_callable.size())
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "add"),
        "add should be callable at ref")
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "size"),
        "size should be callable at ref")
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "describe"),
        "describe should be callable at ref")
    | let err: String val =>
      h.fail("type_api from_position failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiConstructorsTest is UnitTest
  fun name(): String => "type_api/constructors"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    match TypeAPIQuery.from_type(session, "CapMethods", "val")
    | let result: TypeAPIResult val =>
      h.assert_eq[USize](2, result.constructors.size())

      var has_create = false
      var has_from_value = false
      for c in result.constructors.values() do
        h.assert_eq[String val]("new", c.kind)
        if c.name == "create" then has_create = true end
        if c.name == "from_value" then has_from_value = true end
      end
      h.assert_true(has_create, "expected 'create' constructor")
      h.assert_true(has_from_value, "expected 'from_value' constructor")
    | let err: String val =>
      h.fail("type_api constructors failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiTraitsTest is UnitTest
  fun name(): String => "type_api/traits"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Collector is Describable — single trait
    match TypeAPIQuery.from_type(session, "Collector", "ref")
    | let result: TypeAPIResult val =>
      h.assert_eq[USize](1, result.traits.size())
      try
        h.assert_true(
          result.traits(0)?.contains("Describable"),
          "trait should contain Describable: " + result.traits(0)?)
      else
        h.fail("error accessing trait")
      end
    | let err: String val =>
      h.fail("type_api traits for Collector failed: " + err)
    end

    // SteppableCollector is (Describable & Steppable) — two traits
    match TypeAPIQuery.from_type(session, "SteppableCollector", "ref")
    | let result: TypeAPIResult val =>
      h.assert_eq[USize](2, result.traits.size())
      var has_describable = false
      var has_steppable = false
      for t in result.traits.values() do
        if t.contains("Describable") then has_describable = true end
        if t.contains("Steppable") then has_steppable = true end
      end
      h.assert_true(has_describable,
        "expected Describable in traits")
      h.assert_true(has_steppable,
        "expected Steppable in traits")
    | let err: String val =>
      h.fail("type_api traits for SteppableCollector failed: " + err)
    end

    // Worker (actor) — no provides list
    match TypeAPIQuery.from_type(session, "Worker", "tag")
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("actor", result.type_kind)
      h.assert_eq[USize](0, result.traits.size())
    | let err: String val =>
      h.fail("type_api traits for Worker failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiUnionTest is UnitTest
  fun name(): String => "type_api/union"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // union_val.describe() at line 175 — union_val is
    // (Collector ref | SteppableCollector ref)
    match TypeAPIQuery.from_position(session, fixture_file, 175, 5)
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("union", result.type_kind)

      // Both Collector and SteppableCollector have describe()
      // with matching signatures — full signature preserved
      h.assert_eq[USize](1, result.callable.size())
      try
        let m = result.callable(0)?
        h.assert_eq[String val]("describe", m.name)
        h.assert_eq[String val]("fun", m.kind)
        h.assert_eq[String val]("box", m.receiver_cap)
        h.assert_true(
          m.return_type.contains("String"),
          "return type should contain String: " + m.return_type)
        h.assert_eq[USize](0, m.params.size())
        h.assert_true(
          m.source_type.contains(" | "),
          "union method source_type should be the union type")
      else
        h.fail("expected one callable method in union intersection")
      end

      h.assert_false(
        _FixtureHelper.has_method(result.callable, "add"),
        "add should not be in union intersection")
      h.assert_false(
        _FixtureHelper.has_method(result.callable, "size"),
        "size should not be in union intersection")
      h.assert_false(
        _FixtureHelper.has_method(result.callable, "step"),
        "step should not be in union intersection")

      h.assert_eq[USize](0, result.not_callable.size())
      h.assert_eq[USize](0, result.constructors.size())
      h.assert_eq[USize](0, result.traits.size())
    | let err: String val =>
      h.fail("type_api union failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiUnionDivergentTest is UnitTest
  fun name(): String => "type_api/union_divergent"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // divergent at line 178 is (DivergentA ref | DivergentB ref)
    match TypeAPIQuery.from_position(session, fixture_file, 178, 5)
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("union", result.type_kind)

      // shared() has matching signatures across branches
      match _FixtureHelper.find_method(result.callable, "shared")
      | let m: MethodInfo val =>
        h.assert_eq[String val]("box", m.receiver_cap)
        h.assert_true(
          m.return_type.contains("USize"),
          "shared return type should contain USize: " +
            m.return_type)
        h.assert_eq[USize](0, m.params.size())
      | None =>
        h.fail("expected 'shared' in union callable")
      end

      // info() has divergent signatures (String vs USize return)
      match _FixtureHelper.find_method(result.callable, "info")
      | let m: MethodInfo val =>
        h.assert_eq[String val]("", m.receiver_cap)
        h.assert_eq[String val]("", m.return_type)
        h.assert_eq[USize](0, m.params.size())
      | None =>
        h.fail("expected 'info' in union callable")
      end
    | let err: String val =>
      h.fail("type_api union divergent failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiViewpointTest is UnitTest
  fun name(): String => "type_api/viewpoint"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // from_type with a viewpoint-prefixed name strips the prefix
    match TypeAPIQuery.from_type(session, "this->Collector", "ref")
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("class", result.type_kind)
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "add"),
        "Collector should have add method after viewpoint strip")
    | let err: String val =>
      h.fail("type_api viewpoint from_type failed: " + err)
    end

    // Also test stripping a type parameter viewpoint prefix
    match TypeAPIQuery.from_type(session, "A->Collector", "ref")
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("class", result.type_kind)
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "add"),
        "Collector should have add after A-> strip")
    | let err: String val =>
      h.fail("type_api A-> viewpoint from_type failed: " + err)
    end

    // from_position on vp_result (line 181) exercises the
    // tk_arrow handler if the AST produces a viewpoint type
    let fixture_file = _FixtureHelper.fixture_path(h)
    match TypeAPIQuery.from_position(
      session, fixture_file, 181, 5)
    | let result: TypeAPIResult val =>
      h.assert_true(
        result.type_name.contains("String"),
        "viewpoint result should resolve to String: " +
          result.type_name)
    | let err: String val =>
      h.fail("type_api viewpoint from_position failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiEphemeralTest is UnitTest
  fun name(): String => "type_api/ephemeral"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end
    let fixture_file = _FixtureHelper.fixture_path(h)

    // iso_string at line 80 has type String iso
    match TypeAPIQuery.from_position(session, fixture_file, 80, 5)
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("class", result.type_kind)
      h.assert_true(
        result.type_name.contains("String"),
        "ephemeral result should resolve to String: " +
          result.type_name)
      // At iso cap, only tag methods are callable (non-ephemeral)
      // but auto-recovery makes ref methods callable too when
      // args are sendable — the callable list should be non-empty
      h.assert_true(
        result.callable.size() > 0,
        "String iso should have callable methods")
    | let err: String val =>
      h.fail("type_api ephemeral from_position failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiGenericTest is UnitTest
  fun name(): String => "type_api/generic"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    match TypeAPIQuery.from_type(session, "GenericBox", "ref")
    | let result: TypeAPIResult val =>
      h.assert_eq[String val]("class", result.type_kind)
      h.assert_true(
        _FixtureHelper.has_method(result.callable, "get"),
        "GenericBox should have get method")
    | let err: String val =>
      h.fail("type_api for GenericBox failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _TypeApiErrorTest is UnitTest
  fun name(): String => "type_api/error"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Unknown type
    match TypeAPIQuery.from_type(session, "NonExistentType", "ref")
    | let _: TypeAPIResult val =>
      h.fail("expected error for unknown type, got result")
    | let err: String val =>
      h.assert_true(
        err.contains("not found"),
        "error should mention 'not found': " + err)
    end

    // Invalid position
    let fixture_file = _FixtureHelper.fixture_path(h)
    match TypeAPIQuery.from_position(session, fixture_file, 9999, 1)
    | let _: TypeAPIResult val =>
      h.fail("expected error for invalid position, got result")
    | let err: String val =>
      h.assert_true(
        err.contains("no AST node"),
        "error should mention 'no AST node': " + err)
    end

    // Invalid capability
    match TypeAPIQuery.from_type(session, "CapMethods", "bogus")
    | let _: TypeAPIResult val =>
      h.fail("expected error for invalid cap, got result")
    | let err: String val =>
      h.assert_true(
        err.contains("invalid capability"),
        "error should mention invalid capability: " + err)
    end

    session.dispose()

class \nodoc\ iso _CheckCompatibleTest is UnitTest
  fun name(): String => "check/compatible"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Nominal subtyping: Collector ref is Describable ref
    match CheckQuery(session, "Collector ref", "Describable ref")
    | let r: CheckResult val =>
      h.assert_true(r.compatible,
        "Collector ref should be subtype of Describable ref")
      h.assert_eq[String val]("Collector ref", r.source_type)
      h.assert_eq[String val]("Describable ref", r.target_type)
    | let err: String val =>
      h.fail("check Collector/Describable failed: " + err)
    end

    // Structural subtyping: StructuralCountable ref is Countable ref
    match CheckQuery(session, "StructuralCountable ref", "Countable ref")
    | let r: CheckResult val =>
      h.assert_true(r.compatible,
        "StructuralCountable ref should satisfy Countable ref")
    | let err: String val =>
      h.fail("check StructuralCountable/Countable failed: " + err)
    end

    // Self-subtyping: String val is String val
    match CheckQuery(session, "String val", "String val")
    | let r: CheckResult val =>
      h.assert_true(r.compatible,
        "String val should be subtype of String val")
    | let err: String val =>
      h.fail("check String val/String val failed: " + err)
    end

    // Default cap: Collector (defaults to ref) is Describable (defaults to ref)
    match CheckQuery(session, "Collector", "Describable")
    | let r: CheckResult val =>
      h.assert_true(r.compatible,
        "Collector default cap should be subtype of Describable default cap")
      h.assert_true(
        r.source_type.contains("ref"),
        "source should show default cap ref: " + r.source_type)
    | let err: String val =>
      h.fail("check Collector/Describable default cap failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _CheckIncompatibleTest is UnitTest
  fun name(): String => "check/incompatible"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Collector does not implement Countable
    match CheckQuery(session, "Collector ref", "Countable ref")
    | let r: CheckResult val =>
      h.assert_false(r.compatible,
        "Collector ref should not be subtype of Countable ref")
      h.assert_true(
        r.reason.size() > 0,
        "reason should be non-empty for incompatible types")
    | let err: String val =>
      h.fail("check Collector/Countable failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _CheckCapMismatchTest is UnitTest
  fun name(): String => "check/cap_mismatch"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // val is not a subtype of ref
    match CheckQuery(session, "String val", "String ref")
    | let r: CheckResult val =>
      h.assert_false(r.compatible,
        "String val should not be subtype of String ref")
    | let err: String val =>
      h.fail("check String val/ref failed: " + err)
    end

    // ref is a subtype of box
    match CheckQuery(session, "CapMethods ref", "CapMethods box")
    | let r: CheckResult val =>
      h.assert_true(r.compatible,
        "CapMethods ref should be subtype of CapMethods box")
    | let err: String val =>
      h.fail("check CapMethods ref/box failed: " + err)
    end

    // box is not a subtype of ref
    match CheckQuery(session, "CapMethods box", "CapMethods ref")
    | let r: CheckResult val =>
      h.assert_false(r.compatible,
        "CapMethods box should not be subtype of CapMethods ref")
    | let err: String val =>
      h.fail("check CapMethods box/ref failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _CheckSuggestionsTest is UnitTest
  fun name(): String => "check/suggestions"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // ref -> val cap mismatch should generate suggestions
    match CheckQuery(session, "CapMethods ref", "CapMethods val")
    | let r: CheckResult val =>
      h.assert_false(r.compatible,
        "CapMethods ref should not be subtype of CapMethods val")
      h.assert_true(
        r.suggestions.size() > 0,
        "cap mismatch should produce suggestions")
      var has_recover = false
      var has_consume = false
      for s in r.suggestions.values() do
        if s.contains("recover") then has_recover = true end
        if s.contains("consume") then has_consume = true end
      end
      h.assert_true(has_recover,
        "suggestions should include recover advice")
      h.assert_true(has_consume,
        "suggestions should include consume advice")
    | let err: String val =>
      h.fail("check suggestions failed: " + err)
    end

    // Incompatible types (different types) should get suggestions too
    match CheckQuery(session, "Collector ref", "Countable ref")
    | let r: CheckResult val =>
      h.assert_true(
        r.suggestions.size() > 0,
        "incompatible different types should produce suggestions")
    | let err: String val =>
      h.fail("check different-type suggestions failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _CheckErrorTest is UnitTest
  fun name(): String => "check/error"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Unknown source type
    match CheckQuery(session, "NonExistentType ref", "String val")
    | let _: CheckResult val =>
      h.fail("expected error for unknown source type")
    | let err: String val =>
      h.assert_true(
        err.contains("not found"),
        "error should mention 'not found': " + err)
    end

    // Unknown target type
    match CheckQuery(session, "String val", "NonExistentType ref")
    | let _: CheckResult val =>
      h.fail("expected error for unknown target type")
    | let err: String val =>
      h.assert_true(
        err.contains("not found"),
        "error should mention 'not found': " + err)
    end

    session.dispose()

class \nodoc\ iso _ImplementorsNominalTest is UnitTest
  fun name(): String => "implementors/nominal"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Describable trait: Collector and SteppableCollector
    match ImplementorsQuery(session, "Describable")
    | let r: ImplementorsResult val =>
      h.assert_eq[String val]("Describable", r.type_name)

      var has_collector = false
      var has_steppable = false
      for n in r.nominal.values() do
        if n == "Collector" then has_collector = true end
        if n == "SteppableCollector" then has_steppable = true end
      end
      h.assert_true(has_collector,
        "Collector should be nominal implementor of Describable")
      h.assert_true(has_steppable,
        "SteppableCollector should be nominal implementor of Describable")

      // Results should be sorted
      var prev: String val = ""
      for n in r.nominal.values() do
        h.assert_true(n >= prev,
          "nominal results not sorted: '" + prev + "' before '" + n + "'")
        prev = n
      end
    | let err: String val =>
      h.fail("implementors Describable failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ImplementorsStructuralTest is UnitTest
  fun name(): String => "implementors/structural"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Countable interface: CountableItem nominal, StructuralCountable structural
    match ImplementorsQuery(session, "Countable")
    | let r: ImplementorsResult val =>
      var has_nominal = false
      for n in r.nominal.values() do
        if n == "CountableItem" then has_nominal = true end
      end
      h.assert_true(has_nominal,
        "CountableItem should be nominal implementor of Countable")

      var has_structural = false
      for n in r.structural.values() do
        if n == "StructuralCountable" then has_structural = true end
      end
      h.assert_true(has_structural,
        "StructuralCountable should be structural implementor of Countable")

      // Primitives with matching methods are found structurally
      var has_primitive = false
      for n in r.structural.values() do
        if n == "StructuralPrimitive" then has_primitive = true end
      end
      h.assert_true(has_primitive,
        "StructuralPrimitive should be structural implementor of Countable")

      // StructuralCountable should NOT be in nominal
      var structural_in_nominal = false
      for n in r.nominal.values() do
        if n == "StructuralCountable" then
          structural_in_nominal = true
        end
      end
      h.assert_false(structural_in_nominal,
        "StructuralCountable should not appear in nominal list")
    | let err: String val =>
      h.fail("implementors Countable failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ImplementorsIntersectionProvidesTest is UnitTest
  fun name(): String => "implementors/intersection_provides"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Steppable trait: SteppableCollector provides (Describable & Steppable)
    match ImplementorsQuery(session, "Steppable")
    | let r: ImplementorsResult val =>
      var has_sc = false
      for n in r.nominal.values() do
        if n == "SteppableCollector" then has_sc = true end
      end
      h.assert_true(has_sc,
        "SteppableCollector should be found through intersection provides")
    | let err: String val =>
      h.fail("implementors Steppable failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ImplementorsPrivateFilterTest is UnitTest
  fun name(): String => "implementors/private_filter"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Greetable trait: GreetableUser nominal, no private types
    match ImplementorsQuery(session, "Greetable")
    | let r: ImplementorsResult val =>
      var has_greetable_user = false
      for n in r.nominal.values() do
        if n == "GreetableUser" then has_greetable_user = true end
        h.assert_false(
          try n(0)? == '_' else false end,
          "private type should not appear: " + n)
      end
      h.assert_true(has_greetable_user,
        "GreetableUser should be implementor of Greetable")
    | let err: String val =>
      h.fail("implementors Greetable failed: " + err)
    end

    session.dispose()

class \nodoc\ iso _ImplementorsErrorTest is UnitTest
  fun name(): String => "implementors/error"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Non-trait type
    match ImplementorsQuery(session, "Collector")
    | let _: ImplementorsResult val =>
      h.fail("expected error for non-trait type")
    | let err: String val =>
      h.assert_true(
        err.contains("not a trait or interface"),
        "error should say not a trait or interface: " + err)
    end

    // Unknown type
    match ImplementorsQuery(session, "NonExistentType")
    | let _: ImplementorsResult val =>
      h.fail("expected error for unknown type")
    | let err: String val =>
      h.assert_true(
        err.contains("not found"),
        "error should mention 'not found': " + err)
    end

    session.dispose()

primitive \nodoc\ _FixtureHelper
  fun compile(h: TestHelper): (CompileSession, Program val) ? =>
    let fixture_dir =
      Path.join(Path.dir(__loc.file()), "fixture")
    let pony_path =
      match \exhaustive\ PonyPath(h.env)
      | let pp: String => pp
      | None =>
        h.fail("PONYPATH not set")
        error
      end

    let session = CompileSession(
      FilePath(FileAuth(h.env.root), fixture_dir),
      pony_path
      where limit = PassExpr)

    match \exhaustive\ session.program()
    | let prog: Program val =>
      (session, prog)
    | None =>
      let errs = session.errors()
      let msg =
        try errs(0)?.msg
        else "unknown error"
        end
      h.fail("fixture compilation failed: " + msg)
      session.dispose()
      error
    end

  fun fixture_path(h: TestHelper): String =>
    Path.join(Path.dir(__loc.file()), "fixture/main.pony")

  fun find_scope_entry(
    result: ScopeResult val,
    target: String val)
    : (ScopeEntry val | None)
  =>
    for entry in result.entries.values() do
      if entry.name == target then return entry end
    end

  fun find_export(
    result: ExportsResult val,
    target: String val)
    : (ExportEntry val | None)
  =>
    for entry in result.entries.values() do
      if entry.name == target then return entry end
    end

  fun find_method(
    methods: Array[MethodInfo val] val,
    target: String val)
    : (MethodInfo val | None)
  =>
    for m in methods.values() do
      if m.name == target then return m end
    end

  fun has_method(
    methods: Array[MethodInfo val] val,
    target: String val)
    : Bool
  =>
    for m in methods.values() do
      if m.name == target then return true end
    end
    false

  fun has_not_callable(
    not_callable: Array[(MethodInfo val, String val)] val,
    target: String val)
    : Bool
  =>
    for (m, _) in not_callable.values() do
      if m.name == target then return true end
    end
    false

  fun find_type_def(
    program: Program val,
    type_name: String val)
    : (AST box | None)
  =>
    for pkg in program.packages() do
      for mod in pkg.modules() do
        let mod_ast = mod.ast
        var child: (AST box | None) = mod_ast.child()
        while true do
          match child
          | let node: AST box =>
            if TokenIds.is_entity(node.id()) then
              try
                match node(0)?.token_value()
                | let n: String val if n == type_name => return node
                end
              end
            end
            child = node.sibling()
          | None => break
          end
        end
      end
    end

class \nodoc\ iso _CapSubtypingBasicTest is UnitTest
  fun name(): String => "cap_subtyping/basic"

  fun apply(h: TestHelper) =>
    // ref <: box <: tag
    h.assert_true(CapSubtyping.is_sub("ref", "box"),
      "ref should be subtype of box")
    h.assert_true(CapSubtyping.is_sub("box", "tag"),
      "box should be subtype of tag")
    h.assert_true(CapSubtyping.is_sub("ref", "tag"),
      "ref should be subtype of tag (transitive)")

    // val <: box <: tag
    h.assert_true(CapSubtyping.is_sub("val", "box"),
      "val should be subtype of box")
    h.assert_true(CapSubtyping.is_sub("val", "tag"),
      "val should be subtype of tag")

    // iso <: tag (non-ephemeral iso can only become tag)
    h.assert_true(CapSubtyping.is_sub("iso", "tag"),
      "iso should be subtype of tag")

    // trn <: box (non-ephemeral trn can only become box)
    h.assert_true(CapSubtyping.is_sub("trn", "box"),
      "trn should be subtype of box")

    // trn <: tag
    h.assert_true(CapSubtyping.is_sub("trn", "tag"),
      "trn <: tag")

    // Negative cases
    h.assert_false(CapSubtyping.is_sub("box", "ref"),
      "box should not be subtype of ref")
    h.assert_false(CapSubtyping.is_sub("tag", "val"),
      "tag should not be subtype of val")
    h.assert_false(CapSubtyping.is_sub("val", "ref"),
      "val should not be subtype of ref")
    h.assert_false(CapSubtyping.is_sub("box", "val"),
      "box should not be subtype of val")
    h.assert_false(CapSubtyping.is_sub("ref", "val"),
      "ref should not be subtype of val")
    h.assert_false(CapSubtyping.is_sub("iso", "ref"),
      "non-ephemeral iso should not be subtype of ref")
    h.assert_false(CapSubtyping.is_sub("trn", "ref"),
      "non-ephemeral trn should not be subtype of ref")

    // Reflexive
    h.assert_true(CapSubtyping.is_sub("ref", "ref"), "ref <: ref")
    h.assert_true(CapSubtyping.is_sub("val", "val"), "val <: val")
    h.assert_true(CapSubtyping.is_sub("box", "box"), "box <: box")
    h.assert_true(CapSubtyping.is_sub("tag", "tag"), "tag <: tag")
    h.assert_true(CapSubtyping.is_sub("iso", "iso"), "iso <: iso")
    h.assert_true(CapSubtyping.is_sub("trn", "trn"), "trn <: trn")

class \nodoc\ iso _CapSubtypingEphemeralTest is UnitTest
  fun name(): String => "cap_subtyping/ephemeral"

  fun apply(h: TestHelper) =>
    // iso^ can become anything
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("iso", true, "ref", false),
      "iso^ should be subtype of ref")
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("iso", true, "val", false),
      "iso^ should be subtype of val")
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("iso", true, "box", false),
      "iso^ should be subtype of box")
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("iso", true, "trn", false),
      "iso^ should be subtype of trn")

    // trn^ can become ref, val, box
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("trn", true, "ref", false),
      "trn^ should be subtype of ref")
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("trn", true, "val", false),
      "trn^ should be subtype of val")
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("trn", true, "box", false),
      "trn^ should be subtype of box")

    // trn^ cannot become iso
    h.assert_false(
      CapSubtyping.is_sub_ephemeral("trn", true, "iso", false),
      "trn^ should not be subtype of iso")

    // Ephemeral on both sides
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("iso", true, "ref", true),
      "iso^ should be subtype of ref^")
    h.assert_true(
      CapSubtyping.is_sub_ephemeral("ref", false, "box", true),
      "ref should be subtype of box^")

class \nodoc\ iso _CapSubtypingInvalidTest is UnitTest
  fun name(): String => "cap_subtyping/invalid"

  fun apply(h: TestHelper) =>
    h.assert_false(CapSubtyping.is_sub("bogus", "ref"),
      "unrecognized cap should return false")
    h.assert_false(CapSubtyping.is_sub("ref", "bogus"),
      "unrecognized cap should return false")
    h.assert_false(CapSubtyping.is_sub("", "ref"),
      "empty cap should return false")

class \nodoc\ iso _MethodEnumeratorOwnTest is UnitTest
  fun name(): String => "method_enumerator/own_methods"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    let type_ast =
      match _FixtureHelper.find_type_def(prog, "CapMethods")
      | let t: AST box => t
      | None => h.fail("CapMethods type not found"); session.dispose(); return
      end

    let all = MethodEnumerator.methods(type_ast, "CapMethods")

    h.assert_eq[USize](8, all.size())

    match _FixtureHelper.find_method(all, "read_only")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("box", m.receiver_cap)
      h.assert_eq[String val]("fun", m.kind)
      h.assert_eq[String val]("CapMethods", m.source_type)
      h.assert_false(m.is_partial, "read_only is not partial")
      h.assert_true(
        m.return_type.contains("USize"),
        "read_only return type should contain USize")
    | None => h.fail("expected 'read_only' in methods")
    end

    match _FixtureHelper.find_method(all, "mutate")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("ref", m.receiver_cap)
      h.assert_eq[String val]("fun", m.kind)
    | None => h.fail("expected 'mutate' in methods")
    end

    match _FixtureHelper.find_method(all, "immutable_op")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("val", m.receiver_cap)
      h.assert_true(
        m.return_type.contains("String"),
        "immutable_op return type should contain String")
    | None => h.fail("expected 'immutable_op' in methods")
    end

    match _FixtureHelper.find_method(all, "identity")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("tag", m.receiver_cap)
      h.assert_true(
        m.return_type.contains("None"),
        "identity return type should contain None")
    | None => h.fail("expected 'identity' in methods")
    end

    match _FixtureHelper.find_method(all, "partial_lookup")
    | let m: MethodInfo val =>
      h.assert_true(m.is_partial, "partial_lookup should be partial")
      h.assert_eq[String val]("box", m.receiver_cap)
    | None => h.fail("expected 'partial_lookup' in methods")
    end

    // Constructors
    match _FixtureHelper.find_method(all, "create")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("new", m.kind)
    | None => h.fail("expected 'create' constructor")
    end

    match _FixtureHelper.find_method(all, "from_value")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("new", m.kind)
    | None => h.fail("expected 'from_value' constructor")
    end

    session.dispose()

class \nodoc\ iso _MethodEnumeratorInheritedTest is UnitTest
  fun name(): String => "method_enumerator/inherited"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    // Collector is Describable — inherits describe() but defines its own
    let collector_ast =
      match _FixtureHelper.find_type_def(prog, "Collector")
      | let t: AST box => t
      | None =>
        h.fail("Collector type not found"); session.dispose(); return
      end

    let collector_methods =
      MethodEnumerator.methods(collector_ast, "Collector")

    // describe is defined on Collector itself — source_type is Collector
    match _FixtureHelper.find_method(collector_methods, "describe")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("Collector", m.source_type)
    | None => h.fail("expected 'describe' in Collector methods")
    end

    // SteppableCollector is (Describable & Steppable)
    let sc_ast =
      match _FixtureHelper.find_type_def(prog, "SteppableCollector")
      | let t: AST box => t
      | None =>
        h.fail("SteppableCollector not found"); session.dispose(); return
      end

    let sc_methods =
      MethodEnumerator.methods(sc_ast, "SteppableCollector")

    // describe comes from SteppableCollector itself
    match _FixtureHelper.find_method(sc_methods, "describe")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("SteppableCollector", m.source_type)
    | None => h.fail("expected 'describe' in SteppableCollector methods")
    end

    // step comes from SteppableCollector itself
    match _FixtureHelper.find_method(sc_methods, "step")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("SteppableCollector", m.source_type)
    | None => h.fail("expected 'step' in SteppableCollector methods")
    end

    // GreetableUser overrides greet() but inherits farewell()
    let gu_ast =
      match _FixtureHelper.find_type_def(prog, "GreetableUser")
      | let t: AST box => t
      | None =>
        h.fail("GreetableUser not found"); session.dispose(); return
      end

    let gu_methods =
      MethodEnumerator.methods(gu_ast, "GreetableUser")

    match _FixtureHelper.find_method(gu_methods, "greet")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("GreetableUser", m.source_type)
    | None => h.fail("expected 'greet' in GreetableUser methods")
    end

    // Trait default bodies appear as own methods in the AST
    match _FixtureHelper.find_method(gu_methods, "farewell")
    | let m: MethodInfo val =>
      h.assert_eq[String val]("GreetableUser", m.source_type)
    | None => h.fail("expected 'farewell' in GreetableUser methods")
    end

    session.dispose()

class \nodoc\ iso _MethodEnumeratorParamsTest is UnitTest
  fun name(): String => "method_enumerator/params"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    let type_ast =
      match _FixtureHelper.find_type_def(prog, "CapMethods")
      | let t: AST box => t
      | None => h.fail("CapMethods not found"); session.dispose(); return
      end

    let all = MethodEnumerator.methods(type_ast, "CapMethods")

    // with_default has two params, second has a default
    match _FixtureHelper.find_method(all, "with_default")
    | let m: MethodInfo val =>
      h.assert_eq[USize](2, m.params.size())
      try
        let p1 = m.params(0)?
        h.assert_eq[String val]("a", p1.name)
        h.assert_true(
          p1.param_type.contains("USize"),
          "first param type should contain USize")
        h.assert_false(p1.has_default, "first param has no default")
      else
        h.fail("error accessing first param")
      end
      try
        let p2 = m.params(1)?
        h.assert_eq[String val]("b", p2.name)
        h.assert_true(
          p2.param_type.contains("USize"),
          "second param type should contain USize")
        h.assert_true(p2.has_default, "second param has default")
      else
        h.fail("error accessing second param")
      end
    | None => h.fail("expected 'with_default' in methods")
    end

    // read_only has no params
    match _FixtureHelper.find_method(all, "read_only")
    | let m: MethodInfo val =>
      h.assert_eq[USize](0, m.params.size())
    | None => h.fail("expected 'read_only' in methods")
    end

    // mutate has one param
    match _FixtureHelper.find_method(all, "mutate")
    | let m: MethodInfo val =>
      h.assert_eq[USize](1, m.params.size())
      try
        let p = m.params(0)?
        h.assert_eq[String val]("v", p.name)
      else
        h.fail("error accessing mutate param")
      end
    | None => h.fail("expected 'mutate' in methods")
    end

    session.dispose()

class \nodoc\ iso _MethodEnumeratorCallableAtTest is UnitTest
  fun name(): String => "method_enumerator/callable_at"

  fun apply(h: TestHelper) =>
    (let session, let prog) =
      try _FixtureHelper.compile(h)?
      else h.fail("fixture compilation failed"); return
      end

    let type_ast =
      match _FixtureHelper.find_type_def(prog, "CapMethods")
      | let t: AST box => t
      | None => h.fail("CapMethods not found"); session.dispose(); return
      end

    let all = MethodEnumerator.methods(type_ast, "CapMethods")

    // At val: box and tag methods callable, ref not callable
    (let val_callable, let val_not) =
      MethodEnumerator.callable_at(all, "val")

    h.assert_true(
      _FixtureHelper.has_method(val_callable, "read_only"),
      "read_only (box) should be callable at val")
    h.assert_true(
      _FixtureHelper.has_method(val_callable, "identity"),
      "identity (tag) should be callable at val")
    h.assert_true(
      _FixtureHelper.has_method(val_callable, "immutable_op"),
      "immutable_op (val) should be callable at val")
    h.assert_true(
      _FixtureHelper.has_not_callable(val_not, "mutate"),
      "mutate (ref) should not be callable at val")

    // At ref: ref, box, tag callable; val not callable
    (let ref_callable, let ref_not) =
      MethodEnumerator.callable_at(all, "ref")

    h.assert_true(
      _FixtureHelper.has_method(ref_callable, "read_only"),
      "read_only (box) should be callable at ref")
    h.assert_true(
      _FixtureHelper.has_method(ref_callable, "mutate"),
      "mutate (ref) should be callable at ref")
    h.assert_true(
      _FixtureHelper.has_method(ref_callable, "identity"),
      "identity (tag) should be callable at ref")
    h.assert_true(
      _FixtureHelper.has_not_callable(ref_not, "immutable_op"),
      "immutable_op (val) should not be callable at ref")

    // At tag: only tag methods callable
    (let tag_callable, let tag_not) =
      MethodEnumerator.callable_at(all, "tag")

    h.assert_true(
      _FixtureHelper.has_method(tag_callable, "identity"),
      "identity (tag) should be callable at tag")
    h.assert_true(
      _FixtureHelper.has_not_callable(tag_not, "read_only"),
      "read_only (box) should not be callable at tag")
    h.assert_true(
      _FixtureHelper.has_not_callable(tag_not, "mutate"),
      "mutate (ref) should not be callable at tag")

    // Constructors should not appear in callable or not_callable
    h.assert_false(
      _FixtureHelper.has_method(val_callable, "create"),
      "constructors excluded from callable_at")
    h.assert_false(
      _FixtureHelper.has_not_callable(val_not, "create"),
      "constructors excluded from callable_at")

    // Behaviors are always callable regardless of receiver cap
    let worker_ast =
      match _FixtureHelper.find_type_def(prog, "Worker")
      | let t: AST box => t
      | None => h.fail("Worker not found"); session.dispose(); return
      end

    let worker_all = MethodEnumerator.methods(worker_ast, "Worker")
    (let worker_callable, _) =
      MethodEnumerator.callable_at(worker_all, "tag")
    h.assert_true(
      _FixtureHelper.has_method(worker_callable, "work"),
      "behavior 'work' should be callable at tag")

    session.dispose()
