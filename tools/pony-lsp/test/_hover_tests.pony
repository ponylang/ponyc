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
    test(_HoverFormatFieldTest)
    test(_HoverFormatFieldWithTypeTest)

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
    let info = MethodInfo("fun", "get_value", "(x: U32)", ": String")
    let result = HoverFormatter.format_method(info)
    h.assert_eq[String]("```pony\nfun get_value(x: U32): String\n```", result)

class \nodoc\ iso _HoverFormatMethodWithDocstringTest is UnitTest
  fun name(): String => "hover/format_method_with_docstring"

  fun apply(h: TestHelper) =>
    let info = MethodInfo("be", "process", "(data: Array[U8] val)", "", "Process data asynchronously.")
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
