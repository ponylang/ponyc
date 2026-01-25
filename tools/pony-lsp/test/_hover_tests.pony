use "pony_test"
use "../workspace"

primitive _HoverTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_HoverFormatEntityTest)
    test(_HoverFormatEntityWithTypeParamsTest)
    test(_HoverFormatEntityWithDocstringTest)
    test(_HoverFormatMethodTest)
    test(_HoverFormatMethodWithReturnTypeTest)
    test(_HoverFormatMethodWithDocstringTest)
    test(_HoverFormatMethodWithTypeParamsTest)
    test(_HoverFormatFieldTest)
    test(_HoverFormatFieldWithTypeTest)
    test(_HoverFormatEntityWithMultipleTypeParamsTest)
    test(_HoverFormatFieldWithGenericTypeTest)
    test(_HoverFormatMethodWithArrowTypeTest)
    test(_HoverFormatFieldWithUnionTypeTest)
    test(_HoverFormatFieldWithTupleTypeTest)
    test(_HoverFormatMethodWithCompleteSignatureTest)
    test(_HoverFormatEntityWithTypeParamsAndDocstringTest)
    test(_HoverFormatFieldWithNestedGenericTypeTest)

class \nodoc\ iso _HoverFormatEntityTest is UnitTest
  fun name(): String => "hover/format_entity"

  fun apply(h: TestHelper) =>
    let info = EntityInfo("class", "MyClass")
    let result = HoverFormatter.format_entity(info)
    h.assert_eq[String]("```pony\nclass MyClass\n```", result)

class \nodoc\ iso _HoverFormatEntityWithTypeParamsTest is UnitTest
  fun name(): String => "hover/format_entity_with_type_params"

  fun apply(h: TestHelper) =>
    let info = EntityInfo("class", "MyClass", "[T: Any val]")
    let result = HoverFormatter.format_entity(info)
    h.assert_eq[String]("```pony\nclass MyClass[T: Any val]\n```", result)

class \nodoc\ iso _HoverFormatEntityWithDocstringTest is UnitTest
  fun name(): String => "hover/format_entity_with_docstring"

  fun apply(h: TestHelper) =>
    let info = EntityInfo("actor", "MyActor", "", "A sample actor.")
    let result = HoverFormatter.format_entity(info)
    h.assert_eq[String]("```pony\nactor MyActor\n```\n\nA sample actor.", result)

class \nodoc\ iso _HoverFormatMethodTest is UnitTest
  fun name(): String => "hover/format_method"

  fun apply(h: TestHelper) =>
    let info = MethodInfo("fun", "my_method")
    let result = HoverFormatter.format_method(info)
    h.assert_eq[String]("```pony\nfun my_method()\n```", result)

class \nodoc\ iso _HoverFormatMethodWithReturnTypeTest is UnitTest
  fun name(): String => "hover/format_method_with_return_type"

  fun apply(h: TestHelper) =>
    let info = MethodInfo("fun", "get_value", "", "(x: U32)", ": String")
    let result = HoverFormatter.format_method(info)
    h.assert_eq[String]("```pony\nfun get_value(x: U32): String\n```", result)

class \nodoc\ iso _HoverFormatMethodWithDocstringTest is UnitTest
  fun name(): String => "hover/format_method_with_docstring"

  fun apply(h: TestHelper) =>
    let info = MethodInfo("be", "process", "", "(data: Array[U8] val)", "", "Process data asynchronously.")
    let result = HoverFormatter.format_method(info)
    h.assert_eq[String]("```pony\nbe process(data: Array[U8] val)\n```\n\nProcess data asynchronously.", result)

class \nodoc\ iso _HoverFormatFieldTest is UnitTest
  fun name(): String => "hover/format_field"

  fun apply(h: TestHelper) =>
    let info = FieldInfo("let", "name")
    let result = HoverFormatter.format_field(info)
    h.assert_eq[String]("```pony\nlet name\n```", result)

class \nodoc\ iso _HoverFormatFieldWithTypeTest is UnitTest
  fun name(): String => "hover/format_field_with_type"

  fun apply(h: TestHelper) =>
    let info = FieldInfo("var", "count", ": U32")
    let result = HoverFormatter.format_field(info)
    h.assert_eq[String]("```pony\nvar count: U32\n```", result)

class \nodoc\ iso _HoverFormatMethodWithTypeParamsTest is UnitTest
  fun name(): String => "hover/format_method_with_type_params"

  fun apply(h: TestHelper) =>
    let info = MethodInfo("fun", "map", "[U: Any val]", "(f: {(T): U} val)", ": U")
    let result = HoverFormatter.format_method(info)
    h.assert_eq[String]("```pony\nfun map[U: Any val](f: {(T): U} val): U\n```", result)

class \nodoc\ iso _HoverFormatEntityWithMultipleTypeParamsTest is UnitTest
  fun name(): String => "hover/format_entity_with_multiple_type_params"

  fun apply(h: TestHelper) =>
    let info = EntityInfo("class", "Pair", "[A: Any val, B: Any val]")
    let result = HoverFormatter.format_entity(info)
    h.assert_eq[String]("```pony\nclass Pair[A: Any val, B: Any val]\n```", result)

class \nodoc\ iso _HoverFormatFieldWithGenericTypeTest is UnitTest
  fun name(): String => "hover/format_field_with_generic_type"

  fun apply(h: TestHelper) =>
    let info = FieldInfo("let", "items", ": Array[String val] ref")
    let result = HoverFormatter.format_field(info)
    h.assert_eq[String]("```pony\nlet items: Array[String val] ref\n```", result)

class \nodoc\ iso _HoverFormatMethodWithArrowTypeTest is UnitTest
  fun name(): String => "hover/format_method_with_arrow_type"

  fun apply(h: TestHelper) =>
    let info = MethodInfo("fun", "compare", "", "(that: box->T)", ": I32 val")
    let result = HoverFormatter.format_method(info)
    h.assert_eq[String]("```pony\nfun compare(that: box->T): I32 val\n```", result)

class \nodoc\ iso _HoverFormatFieldWithUnionTypeTest is UnitTest
  fun name(): String => "hover/format_field_with_union_type"

  fun apply(h: TestHelper) =>
    let info = FieldInfo("let", "value", ": (String val | U32 val | None val)")
    let result = HoverFormatter.format_field(info)
    h.assert_eq[String]("```pony\nlet value: (String val | U32 val | None val)\n```", result)

class \nodoc\ iso _HoverFormatFieldWithTupleTypeTest is UnitTest
  fun name(): String => "hover/format_field_with_tuple_type"

  fun apply(h: TestHelper) =>
    let info = FieldInfo("let", "pair", ": (String val, U32 val)")
    let result = HoverFormatter.format_field(info)
    h.assert_eq[String]("```pony\nlet pair: (String val, U32 val)\n```", result)

class \nodoc\ iso _HoverFormatMethodWithCompleteSignatureTest is UnitTest
  fun name(): String => "hover/format_method_with_complete_signature"

  fun apply(h: TestHelper) =>
    let info = MethodInfo("fun", "transform", "[U: Any val]", "(data: Array[T] ref)", ": Array[U] ref", "Transform elements.")
    let result = HoverFormatter.format_method(info)
    h.assert_eq[String]("```pony\nfun transform[U: Any val](data: Array[T] ref): Array[U] ref\n```\n\nTransform elements.", result)

class \nodoc\ iso _HoverFormatEntityWithTypeParamsAndDocstringTest is UnitTest
  fun name(): String => "hover/format_entity_with_type_params_and_docstring"

  fun apply(h: TestHelper) =>
    let info = EntityInfo("class", "Container", "[T: Any val]", "A generic container.")
    let result = HoverFormatter.format_entity(info)
    h.assert_eq[String]("```pony\nclass Container[T: Any val]\n```\n\nA generic container.", result)

class \nodoc\ iso _HoverFormatFieldWithNestedGenericTypeTest is UnitTest
  fun name(): String => "hover/format_field_with_nested_generic_type"

  fun apply(h: TestHelper) =>
    let info = FieldInfo("let", "nested", ": Array[Array[String val] ref] ref")
    let result = HoverFormatter.format_field(info)
    h.assert_eq[String]("```pony\nlet nested: Array[Array[String val] ref] ref\n```", result)
