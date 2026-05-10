use ".."
use "pony_test"
use "files"
use "json"
use "collections"

primitive _HoverIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_HoverIntegrationClassTest.create(server))
    test(_HoverIntegrationActorTest.create(server))
    test(_HoverIntegrationAliasTest.create(server))
    test(_HoverIntegrationInterfaceTest.create(server))
    test(_HoverIntegrationPrimitiveTest.create(server))
    test(_HoverIntegrationTraitTest.create(server))
    test(_HoverIntegrationStructTest.create(server))
    test(_HoverIntegrationTypeInferenceTest.create(server))
    test(_HoverIntegrationReceiverCapabilityTest.create(server))
    test(_HoverIntegrationComplexTypesTest.create(server))
    test(_HoverIntegrationFunctionTest.create(server))
    test(_HoverIntegrationGenericsTest.create(server))
    test(_HoverIntegrationLiteralsTest.create(server))
    test(_HoverIntegrationCapKeywordTest.create(server))
    test(_HoverIntegrationFieldDocstringTest.create(server))

class \nodoc\ iso _HoverIntegrationLiteralsTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/literals"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_literals.pony",
      [ // variable declarations show their types
        _HoverChecker(11, 8, ["let integer: U32 val"])
        _HoverChecker(12, 8, ["let hex: U32 val"])
        _HoverChecker(13, 8, ["let binary: U32 val"])
        _HoverChecker(14, 8, ["let float_val: F64 val"])
        _HoverChecker(15, 8, ["let string_val: String val"])
        _HoverChecker(16, 8, ["let char_val: U32 val"])
        _HoverChecker(17, 8, ["let bool_true: Bool val"])
        _HoverChecker(18, 8, ["let bool_false: Bool val"])
        _HoverChecker(19, 8, ["let array_val: Array[U32 val] ref"])
        // no hover on literal values
        _HoverChecker(11, 23, [])
        _HoverChecker(12, 19, [])
        _HoverChecker(13, 22, [])
        _HoverChecker(14, 25, [])
        _HoverChecker(15, 29, [])
        _HoverChecker(16, 24, [])
        _HoverChecker(17, 26, [])
        _HoverChecker(18, 27, [])
        _HoverChecker(19, 33, [])])

class \nodoc\ iso _HoverIntegrationClassTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/class"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_class.pony",
      [ _HoverChecker(
          0,
          6,
          ["class _Class"; "A simple class for exercising LSP hover."])
        // field declarations
        _HoverChecker(4, 6, ["let field_name: String val"])
        _HoverChecker(5, 6, ["var mutable_field: U32 val"])
        _HoverChecker(
          6,
          8,
          ["embed embedded_field: Array[String val] ref"])
        // no hover on field declaration keywords
        _HoverChecker(4, 2, [])
        _HoverChecker(4, 4, [])
        _HoverChecker(5, 2, [])
        _HoverChecker(5, 4, [])
        _HoverChecker(6, 2, [])
        _HoverChecker(6, 6, [])
        // constructor declaration
        _HoverChecker(8, 6, ["new ref create(name: String val)"])
        // field usages
        _HoverChecker(9, 4, ["let field_name: String val"])
        _HoverChecker(10, 4, ["var mutable_field: U32 val"])
        _HoverChecker(16, 4, ["let field_name: String val"])
        _HoverChecker(28, 4, ["var mutable_field: U32 val"])
        // no hover on 'class' keyword
        _HoverChecker(0, 0, [])
        _HoverChecker(0, 2, [])
        // no hover on 'new' keyword
        _HoverChecker(8, 2, [])
        _HoverChecker(8, 4, [])
        // no hover on receiver capability keyword
        _HoverChecker(24, 6, [])
        _HoverChecker(24, 8, [])
        // no hover on docstring or blank line
        _HoverChecker(1, 2, [])
        _HoverChecker(2, 4, [])
        _HoverChecker(7, 0, [])])

class \nodoc\ iso _HoverIntegrationActorTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/actor"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_actor.pony",
      [ _HoverChecker(
          0,
          6,
          ["actor _Actor"; "A simple actor for exercising LSP hover."])
        _HoverChecker(6, 6, ["new tag create(name': String val)"])
        _HoverChecker(
          9,
          5,
          ["be tag do_something(value: U64 val)"],
          ["): None val"])
        // beref call site: same signature, no return type
        _HoverChecker(
          13,
          4,
          ["be tag do_something(value: U64 val)"],
          ["): None val"])
        // no hover on 'actor' keyword
        _HoverChecker(0, 0, [])
        _HoverChecker(0, 2, [])
        // no hover on 'new' keyword
        _HoverChecker(6, 2, [])
        _HoverChecker(6, 4, [])
        // no hover on 'be' keyword
        _HoverChecker(9, 2, [])
        _HoverChecker(9, 3, [])])

class \nodoc\ iso _HoverIntegrationAliasTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/alias"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_alias.pony",
      [ _HoverChecker(
          0,
          5,
          ["type _Alias"; "A simple alias for exercising LSP hover."])
        // no hover on 'type' keyword
        _HoverChecker(0, 0, [])
        _HoverChecker(0, 2, [])])

class \nodoc\ iso _HoverIntegrationInterfaceTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/interface"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_interface.pony",
      [ _HoverChecker(
          0,
          10,
          [ "interface _Interface"
            "A simple interface for exercising LSP hover."])
        // no hover on 'interface' keyword
        _HoverChecker(0, 0, [])
        _HoverChecker(0, 4, [])
        // no hover on return type capability keyword
        _HoverChecker(4, 31, [])
        _HoverChecker(4, 33, [])])

class \nodoc\ iso _HoverIntegrationPrimitiveTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/primitive"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_primitive.pony",
      [ _HoverChecker(
          0,
          10,
          [ "primitive _Primitive"
            "A simple primitive for exercising LSP hover."])
        // no hover on 'primitive' keyword
        _HoverChecker(0, 0, [])
        _HoverChecker(0, 4, [])])

class \nodoc\ iso _HoverIntegrationTraitTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/trait"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_trait.pony",
      [ _HoverChecker(
          0,
          6,
          ["trait _Trait"; "A simple trait for exercising LSP hover."])
        // no hover on 'trait' keyword
        _HoverChecker(0, 0, [])
        _HoverChecker(0, 2, [])])

class \nodoc\ iso _HoverIntegrationFunctionTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/function"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_function.pony",
      [ _HoverChecker(
          15,
          6,
          [ "fun box helper_method(input: String val): String val"
            "A helper method that processes input."])
        _HoverChecker(
          21,
          6,
          [ "fun box method_with_multiple_params" +
            "(x: U32 val, y: String val, flag: Bool val): String val"
            "Method with multiple parameters for testing."])
        _HoverChecker(
          11,
          23,
          [ "fun box helper_method(input: String val): String val"
            "A helper method that processes input."])
        _HoverChecker(
          12,
          23,
          [ "fun box method_with_multiple_params" +
            "(x: U32 val, y: String val, flag: Bool val): String val"
            "Method with multiple parameters for testing."])
        _HoverChecker(11, 8, ["let result1: String val"])
        _HoverChecker(12, 8, ["let result2: String val"])
        // no hover on local variable declaration keywords
        _HoverChecker(11, 4, [])
        _HoverChecker(11, 6, [])
        _HoverChecker(12, 4, [])
        _HoverChecker(12, 6, [])
        // parameter declarations
        _HoverChecker(15, 20, ["input: String val"])
        _HoverChecker(21, 34, ["x: U32 val"])
        _HoverChecker(21, 42, ["y: String val"])
        _HoverChecker(21, 53, ["flag: Bool val"])
        // parameter usages
        _HoverChecker(19, 4, ["input: String val"])
        _HoverChecker(25, 4, ["y: String val"])
        // ref receiver method
        _HoverChecker(
          27,
          10,
          [ "fun ref mutable_method(value: U32 val)"
            "A method with a ref receiver capability."])
        // no hover on 'fun' keyword or receiver capability keyword
        _HoverChecker(15, 2, [])
        _HoverChecker(15, 4, [])
        _HoverChecker(27, 2, [])
        _HoverChecker(27, 3, [])
        _HoverChecker(27, 4, [])
        _HoverChecker(27, 6, [])
        _HoverChecker(27, 8, [])
        // this reference
        _HoverChecker(11, 18, [])
        // var local declaration and usage
        _HoverChecker(37, 8, ["var counter: U32 val"])
        _HoverChecker(38, 17, ["var counter: U32 val"])
        // no hover on var declaration keyword
        _HoverChecker(37, 4, [])
        _HoverChecker(37, 6, [])])

class \nodoc\ iso _HoverIntegrationStructTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/struct"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_struct.pony",
      [ _HoverChecker(
          0,
          7,
          ["struct _Struct"; "A simple struct for exercising LSP hover."])
        // no hover on 'struct' keyword
        _HoverChecker(0, 0, [])
        _HoverChecker(0, 5, [])])

class \nodoc\ iso _HoverIntegrationTypeInferenceTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/type_inference"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_type_inference.pony",
      [ _HoverChecker(18, 8, ["let inferred_string: String val"])
        _HoverChecker(21, 8, ["let inferred_bool: Bool"])
        _HoverChecker(24, 8, ["let inferred_array: Array[U32 val] ref"])
        _HoverChecker(26, 4, ["let inferred_string: String val"])
        _HoverChecker(26, 22, ["let inferred_bool: Bool"])
        _HoverChecker(26, 47, ["let inferred_array: Array[U32 val] ref"])])

class \nodoc\ iso _HoverIntegrationReceiverCapabilityTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/receiver_capability"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_receiver_capability.pony",
      [ _HoverChecker(5, 10, ["fun box boxed_method(): String val"])
        _HoverChecker(13, 10, ["fun val valued_method(): String val"])
        _HoverChecker(20, 10, ["fun ref mutable_method()"])
        // no hover on receiver capability keywords
        _HoverChecker(5, 6, [])
        _HoverChecker(5, 8, [])
        _HoverChecker(13, 6, [])
        _HoverChecker(13, 8, [])
        _HoverChecker(20, 6, [])
        _HoverChecker(20, 8, [])])

class \nodoc\ iso _HoverIntegrationComplexTypesTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/complex_types"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_complex_types.pony",
      [ _HoverChecker(
          11,
          8,
          ["let union_type: (String val | U32 val | None val)"])
        _HoverChecker(
          12,
          8,
          ["let tuple_type: (String val, U32 val, Bool val)"])
        _HoverChecker(
          13,
          4,
          ["let union_type: (String val | U32 val | None val)"])
        _HoverChecker(
          13,
          26,
          ["let tuple_type: (String val, U32 val, Bool val)"])])

class \nodoc\ iso _HoverIntegrationGenericsTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/generics"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_generics.pony",
      [ // trait _Generics
        _HoverChecker(
          0,
          6,
          [ "trait _Generics[T: _Generics[T] box]"
            "A trait demonstrating recursive type constraints."])
        _HoverChecker(5, 6, ["fun box compare(that: box->T): I32 val"])
        // class _GenericsDemo
        _HoverChecker(
          7,
          6,
          [ "class _GenericsDemo[T: Any val]"
            "Demonstrates hover on generic classes with type parameters."])
        _HoverChecker(12, 6, ["let _value: T"])
        _HoverChecker(
          17,
          6,
          ["fun box get(): T"; "Returns the stored value."])
        _HoverChecker(
          23,
          6,
          [ "fun box with_generic_param[U: Any val](other: U): (T, U)"
            "A generic method with its own type parameter."])
        // class _GenericPair
        _HoverChecker(
          30,
          6,
          [ "class _GenericPair[A: Any val, B: Any val]"
            "Demonstrates multiple type parameters."])
        _HoverChecker(35, 6, ["let first: A"])
        _HoverChecker(36, 6, ["let second: B"])
        _HoverChecker(
          42,
          6,
          ["fun box get_first(): A"; "Returns the first element."])
        _HoverChecker(
          48,
          6,
          ["fun box get_second(): B"; "Returns the second element."])
        // class _GenericContainer
        _HoverChecker(
          54,
          6,
          [ "class _GenericContainer[T: Stringable val]"
            "Demonstrates type constraints on generic parameters."])
        _HoverChecker(59, 6, ["let _items: Array[T] ref"])
        _HoverChecker(
          64,
          10,
          ["fun ref add(item: T)"; "Add an item to the container."])
        _HoverChecker(
          70,
          6,
          [ "fun box get_strings(): Array[String val] ref"
            "Returns string representations of all items."])
        _HoverChecker(75, 8, ["let result: Array[String val] ref"])
        // actor _GenericActor
        _HoverChecker(
          81,
          6,
          [ "actor _GenericActor[T: Any val]"
            "Demonstrates generic actors."])
        _HoverChecker(86, 6, ["var _state: T"])
        _HoverChecker(
          91,
          5,
          [ "be tag update(new_state: T)"
            "Updates the actor's state."],
          ["): None val"])
        _HoverChecker(
          97,
          5,
          [ "be tag process[U: Any val](data: U)"
            "A generic behaviour with its own type parameter."],
          ["): None val"])
        // primitive _GenericHelper
        _HoverChecker(
          104,
          10,
          [ "primitive _GenericHelper"
            "Demonstrates generic methods in primitives."])
        _HoverChecker(
          108,
          6,
          [ "fun box identity[T: Any val](value: T): T"
            "A generic identity function."])
        _HoverChecker(
          115,
          6,
          [ "fun box create_pair[A: Any val, B: Any val](a: A, b: B): (A, B)"
            "Creates a tuple from two values of different types."])
        _HoverChecker(
          121,
          6,
          [ "fun box apply[T: Any val](): Array[T] ref"
            "Creates an empty array of any type."])
        // class _GenericUsageDemo - instantiated generics
        _HoverChecker(
          128,
          6,
          [ "class _GenericUsageDemo"
            "Demonstrates hover on instantiated generic types."])
        _HoverChecker(
          141,
          8,
          ["let pair: _GenericPair[String val, U32 val] ref"])
        _HoverChecker(
          142,
          8,
          ["let container: _GenericContainer[String val] ref"])
        _HoverChecker(143, 8, ["let numbers: _GenericsDemo[U64 val] ref"])
        _HoverChecker(148, 8, ["let first: String val"])
        _HoverChecker(
          148,
          21,
          ["fun box get_first(): A"; "Returns the first element."])
        _HoverChecker(151, 8, ["let result: (U64 val, String val)"])
        _HoverChecker(
          151,
          25,
          [ "fun box with_generic_param[U: Any val](other: U): (T, U)"
            "A generic method with its own type parameter."])
        // no hover on capability keywords in type parameter constraints
        _HoverChecker(0, 32, [])   // trait _Generics[T: _Generics[T] box]
        _HoverChecker(5, 20, [])   // fun compare(that: box->T) -- arrow type
        _HoverChecker(7, 27, [])   // class _GenericsDemo[T: Any val]
        _HoverChecker(23, 32, [])  // fun with_generic_param[U: Any val]
        _HoverChecker(30, 26, [])  // class _GenericPair[A: Any val, ...]
        _HoverChecker(30, 38, [])  // class _GenericPair[..., B: Any val]
        _HoverChecker(54, 38, [])  // class _GenericContainer[T: Stringable val]
        _HoverChecker(64, 6, [])   // fun ref add -- receiver cap
        _HoverChecker(81, 27, [])  // actor _GenericActor[T: Any val]
        _HoverChecker(97, 20, [])  // be process[U: Any val]
        _HoverChecker(108, 22, []) // fun identity[T: Any val]
        _HoverChecker(115, 25, []) // fun create_pair[A: Any val, ...]
        _HoverChecker(115, 37, []) // fun create_pair[..., B: Any val]
        _HoverChecker(121, 19, []) // fun apply[T: Any val]
        ])

class \nodoc\ iso _HoverIntegrationCapKeywordTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/cap_keyword"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_cap_keyword.pony",
      [ // entity default cap: 'ref' in 'class ref _CapKeyword'
        _HoverChecker(0, 6, [])
        _HoverChecker(0, 8, [])
        // field type cap: 'val' in 'let _data: String val'
        _HoverChecker(7, 20, [])
        _HoverChecker(7, 22, [])
        // constructor receiver cap: 'ref' in 'new ref create()'
        _HoverChecker(9, 6, [])
        _HoverChecker(9, 8, [])
        // receiver and return type caps: 'fun val recv_val(): String val'
        _HoverChecker(12, 6, [])   // receiver val
        _HoverChecker(12, 8, [])
        _HoverChecker(12, 29, [])  // return type val
        _HoverChecker(12, 31, [])
        // receiver and parameter type caps: 'fun ref recv_ref(x: String val)'
        _HoverChecker(15, 6, [])   // receiver ref
        _HoverChecker(15, 8, [])
        _HoverChecker(15, 29, [])  // parameter type val
        _HoverChecker(15, 31, [])
        // receiver and arrow type caps: 'fun box arrow_type(x: box->String)'
        _HoverChecker(18, 6, [])   // receiver box
        _HoverChecker(18, 8, [])
        _HoverChecker(18, 24, [])  // arrow type box (viewpoint adaptation)
        _HoverChecker(18, 26, [])
        // local variable type cap: 'let local: String val'
        _HoverChecker(22, 22, [])
        _HoverChecker(22, 24, [])
        // iso receiver cap: 'fun iso iso_method()'
        _HoverChecker(25, 6, [])
        _HoverChecker(25, 8, [])
        // trn receiver cap: 'fun trn trn_method()'
        _HoverChecker(28, 6, [])
        _HoverChecker(28, 8, [])
        // tag receiver cap: 'fun tag tag_method()'
        _HoverChecker(31, 6, [])
        _HoverChecker(31, 8, [])
        // positive: method name resolves on same line as suppressed cap
        _HoverChecker(12, 10, ["fun val recv_val(): String val"])
        // positive: local var name resolves on same line as suppressed cap
        _HoverChecker(22, 10, ["let local: String val"])
        // positive: iso/trn/tag method names resolve
        _HoverChecker(25, 10, ["fun iso iso_method(): None"])
        _HoverChecker(28, 10, ["fun trn trn_method(): None"])
        _HoverChecker(31, 10, ["fun tag tag_method(): None"])])

class \nodoc\ iso _HoverIntegrationFieldDocstringTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/field_docstring"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "hover/_field_docstring.pony",
      [ // declaration site: let field with docstring
        _HoverChecker(
          1,
          6,
          ["let documented_field: String val"; "Has a docstring."])
        // declaration site: var field with default value and docstring
        _HoverChecker(5, 6, ["var counter: U32 val"; "Has a default value."])
        // usage site: docstring shown at reference
        _HoverChecker(
          11,
          4,
          ["let documented_field: String val"; "Has a docstring."])])

class val _HoverChecker
  let _line: I64
  let _character: I64
  let _expected: Array[String] val
  let _not_expected: Array[String] val

  new val create(
    line': I64,
    character': I64,
    expected: Array[String] val,
    not_expected: Array[String] val = recover val Array[String] end)
  =>
    _line = line'
    _character = character'
    _expected = expected
    _not_expected = not_expected

  fun lsp_method(): String =>
    Methods.text_document().hover()

  fun lsp_params(): (None | JsonObject) =>
    JsonObject.update(
      "position",
      JsonObject.update("line", _line).update("character", _character))

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    try
      let value = JsonNav(res.result)("contents")("value").as_string()?
      if _expected.size() == 0 then
        ok = false
        h.log("Expected no hover result but got: " + res.string())
      else
        for s in _expected.values() do
          if not h.assert_true(
            value.contains(s),
            "Expected '" + s + "' in hover, got: " + value)
          then
            ok = false
          end
        end
      end
      for s in _not_expected.values() do
        if not h.assert_true(
          not value.contains(s),
          "Expected '" + s + "' NOT in hover, got: " + value)
        then
          ok = false
        end
      end
    else
      if _expected.size() > 0 then
        ok = false
        let expected_str: String val =
          recover val
            let s = String
            for e in _expected.values() do
              if s.size() > 0 then s.append(", ") end
              s.append("'"); s.append(e); s.append("'")
            end
            s
          end
        h.log("Hover returned null, expected: " + expected_str)
      end
    end
    ok
