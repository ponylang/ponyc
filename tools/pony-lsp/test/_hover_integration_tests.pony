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
    test(_HoverIntegrationTypeInferenceTest.create(server))
    test(_HoverIntegrationReceiverCapabilityTest.create(server))
    test(_HoverIntegrationComplexTypesTest.create(server))
    test(_HoverIntegrationFunctionTest.create(server))
    test(_HoverIntegrationGenericsTest.create(server))
    test(_HoverIntegrationLiteralsTest.create(server))

class \nodoc\ iso _HoverIntegrationLiteralsTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/literals"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_literals.pony"
    let checks: Array[HoverCheck] val =
      [ // variable declarations show their types
        (11, 8, ["let integer: U32 val"])
        (12, 8, ["let hex: U32 val"])
        (13, 8, ["let binary: U32 val"])
        (14, 8, ["let float_val: F64 val"])
        (15, 8, ["let string_val: String val"])
        (16, 8, ["let char_val: U32 val"])
        (17, 8, ["let bool_true: Bool val"])
        (18, 8, ["let bool_false: Bool val"])
        (19, 8, ["let array_val: Array[U32 val] ref"])
        // no hover on literal values
        (11, 23, [])
        (12, 19, [])
        (13, 22, [])
        (14, 25, [])
        (15, 29, [])
        (16, 24, [])
        (17, 26, [])
        (18, 27, [])
        (19, 33, [])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationClassTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/class"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_class.pony"
    let checks: Array[HoverCheck] val =
      [ (0, 6, ["class _Class"; "A simple class for exercising LSP hover."])
        // field declarations
        (4, 6, ["let field_name: String val"])
        (5, 6, ["var mutable_field: U32 val"])
        (6, 8, ["embed embedded_field: Array[String val] ref"])
        // constructor declaration
        (8, 6, ["new ref create(name: String val)"])
        // field usages
        (9, 4, ["let field_name: String val"])
        (10, 4, ["var mutable_field: U32 val"])
        (16, 4, ["let field_name: String val"])
        (28, 4, ["var mutable_field: U32 val"])
        // no hover on docstring or blank line
        (1, 2, [])
        (2, 4, [])
        (7, 0, [])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationActorTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/actor"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_actor.pony"
    let checks: Array[HoverCheck] val =
      [ (0, 6, ["actor _Actor"; "A simple actor for exercising LSP hover."])
        (6, 6, ["new tag create(name': String val)"])
        (9, 5, ["be tag do_something(value: U64 val)"])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationAliasTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/alias"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_alias.pony"
    let checks: Array[HoverCheck] val =
      [(0, 5, ["type _Alias"; "A simple alias for exercising LSP hover."])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationInterfaceTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/interface"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_interface.pony"
    let checks: Array[HoverCheck] val =
      [ (0, 10,
        [ "interface _Interface"
          "A simple interface for exercising LSP hover."])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationPrimitiveTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/primitive"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_primitive.pony"
    let checks: Array[HoverCheck] val =
      [ (0, 10,
        [ "primitive _Primitive"
          "A simple primitive for exercising LSP hover."])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationTraitTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/trait"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_trait.pony"
    let checks: Array[HoverCheck] val =
      [(0, 6, ["trait _Trait"; "A simple trait for exercising LSP hover."])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationFunctionTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/function"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_function.pony"
    let checks: Array[HoverCheck] val =
      [ (15, 6,
        [ "fun box helper_method(input: String val): String val"
          "A helper method that processes input."])
        (21, 6,
        [ "fun box method_with_multiple_params" +
          "(x: U32 val, y: String val, flag: Bool val): String val"
          "Method with multiple parameters for testing."])
        (11, 23,
        [ "fun box helper_method(input: String val): String val"
          "A helper method that processes input."])
        (12, 23,
          [ "fun box method_with_multiple_params" +
            "(x: U32 val, y: String val, flag: Bool val): String val"
            "Method with multiple parameters for testing."])
        (11, 8, ["let result1: String val"])
        (12, 8, ["let result2: String val"])
        // parameter declarations
        (15, 20, ["input: String val"])
        (21, 34, ["x: U32 val"])
        (21, 42, ["y: String val"])
        (21, 53, ["flag: Bool val"])
        // parameter usages
        (19, 4, ["input: String val"])
        (25, 4, ["y: String val"])
        // ref receiver method
        (27, 10,
        [ "fun ref mutable_method(value: U32 val)"
          "A method with a ref receiver capability."])
        // this reference
        (11, 18, [])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationTypeInferenceTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/type_inference"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_type_inference.pony"
    let checks: Array[HoverCheck] val =
      [ (18, 8, ["let inferred_string: String val"])
        (21, 8, ["let inferred_bool: Bool"])
        (24, 8, ["let inferred_array: Array[U32 val] ref"])
        (26, 4, ["let inferred_string: String val"])
        (26, 22, ["let inferred_bool: Bool"])
        (26, 47, ["let inferred_array: Array[U32 val] ref"])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationReceiverCapabilityTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/receiver_capability"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_receiver_capability.pony"
    let checks: Array[HoverCheck] val =
      [ (5, 10, ["fun box boxed_method(): String val"])
        (13, 10, ["fun val valued_method(): String val"])
        (20, 10, ["fun ref mutable_method()"])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationComplexTypesTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/complex_types"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_complex_types.pony"
    let checks: Array[HoverCheck] val =
      [ (11, 8, ["let union_type: (String val | U32 val | None val)"])
        (12, 8, ["let tuple_type: (String val, U32 val, Bool val)"])
        (13, 4, ["let union_type: (String val | U32 val | None val)"])
        (13, 26, ["let tuple_type: (String val, U32 val, Bool val)"])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

class \nodoc\ iso _HoverIntegrationGenericsTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "hover/integration/generics"

  fun apply(h: TestHelper) =>
    let workspace_file = "hover/_generics.pony"
    let checks: Array[HoverCheck] val =
      [ // trait _Generics
        (0, 6,
        [ "trait _Generics[T: _Generics[T] box]"
          "A trait demonstrating recursive type constraints."])
        (5, 6, ["fun box compare(that: box->T): I32 val"])
        // class _GenericsDemo
        (7, 6,
        [ "class _GenericsDemo[T: Any val]"
          "Demonstrates hover on generic classes with type parameters."])
        (12, 6, ["let _value: T"])
        (17, 6,
        [ "fun box get(): T"
          "Returns the stored value."])
        (23, 6,
        [ "fun box with_generic_param[U: Any val](other: U): (T, U)"
          "A generic method with its own type parameter."])
        // class _GenericPair
        (30, 6,
        [ "class _GenericPair[A: Any val, B: Any val]"
          "Demonstrates multiple type parameters."])
        (35, 6, ["let first: A"])
        (36, 6, ["let second: B"])
        (42, 6,
        [ "fun box get_first(): A"
          "Returns the first element."])
        (48, 6,
        [ "fun box get_second(): B"
          "Returns the second element."])
        // class _GenericContainer
        (54, 6,
        [ "class _GenericContainer[T: Stringable val]"
          "Demonstrates type constraints on generic parameters."])
        (59, 6, ["let _items: Array[T] ref"])
        (64, 10,
        [ "fun ref add(item: T)"
          "Add an item to the container."])
        (70, 6,
        [ "fun box get_strings(): Array[String val] ref"
          "Returns string representations of all items."])
        (75, 8, ["let result: Array[String val] ref"])
        // actor _GenericActor
        (81, 6,
        [ "actor _GenericActor[T: Any val]"
          "Demonstrates generic actors."])
        (86, 6, ["var _state: T"])
        (91, 5,
        [ "be tag update(new_state: T): None val"
          "Updates the actor's state."])
        (97, 5,
        [ "be tag process[U: Any val](data: U): None val"
          "A generic behavior with its own type parameter."])
        // primitive _GenericHelper
        (104, 10,
        [ "primitive _GenericHelper"
          "Demonstrates generic methods in primitives."])
        (108, 6,
        [ "fun box identity[T: Any val](value: T): T"
          "A generic identity function."])
        (115, 6,
        [ "fun box create_pair[A: Any val, B: Any val](a: A, b: B): (A, B)"
          "Creates a tuple from two values of different types."])
        (121, 6,
        [ "fun box apply[T: Any val](): Array[T] ref"
          "Creates an empty array of any type."])
        // class _GenericUsageDemo - instantiated generics
        (128, 6,
        [ "class _GenericUsageDemo"
          "Demonstrates hover on instantiated generic types."])
        (141, 8, ["let pair: _GenericPair[String val, U32 val] ref"])
        (142, 8, ["let container: _GenericContainer[String val] ref"])
        (143, 8, ["let numbers: _GenericsDemo[U64 val] ref"])
        (148, 8, ["let first: String val"])
        (148, 21, ["fun box get_first(): A"; "Returns the first element."])
        (151, 8, ["let result: (U64 val, String val)"])
        (151, 25,
        [ "fun box with_generic_param[U: Any val](other: U): (T, U)"
          "A generic method with its own type parameter."])]
    h.long_test(10_000_000_000)
    for (line, character, expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _HoverChecker(expected))
    end

type HoverCheck is (I64, I64, Array[String] val)

class val _HoverChecker
  let _expected: Array[String] val

  new val create(expected: Array[String] val) =>
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().hover()

  fun check(res: ResponseMessage val, h: TestHelper, action: String): Bool =>
    var ok = true
    if _expected.size() == 0 then
      try
        JsonNav(res.result)("contents")("value").as_string()?
        ok = false
        h.log("Expected no hover result but got: " + res.string())
      end
    else
      try
        let value = JsonNav(res.result)("contents")("value").as_string()?
        for s in _expected.values() do
          if not h.assert_true(
            value.contains(s),
            "Expected '" + s + "' in hover, got: " + value)
          then
            ok = false
          end
        end
      else
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
