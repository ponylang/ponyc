use "pony_test"
use doc = ".."

primitive \nodoc\ _DefaultValueTestHelper
  fun find_default(
    prog: doc.DocProgram,
    method_name: String)
    : (String | None)
  =>
    """
    Finds the first parameter's default_value on the named public
    function in the first public type of the root package.
    """
    try
      let entity = prog.packages(0)?.public_types(0)?
      for m in entity.public_functions.values() do
        if m.name == method_name then
          return m.params(0)?.default_value
        end
      end
    end
    None

class \nodoc\ _TestDefaultValueIntLiteral is UnitTest
  fun name(): String => "DefaultValue/int-literal"

  fun apply(h: TestHelper) ? =>
    let prog = _ASTTestHelper.extract(h,
      """
      primitive Foo
        fun apply(n: USize = 42): None => None
      """)?
    h.assert_eq[String](
      _DefaultValueTestHelper.find_default(prog, "apply")
        as String,
      "42")

class \nodoc\ _TestDefaultValueNegativeInt is UnitTest
  fun name(): String => "DefaultValue/negative-int"

  fun apply(h: TestHelper) ? =>
    let prog = _ASTTestHelper.extract(h,
      """
      primitive Foo
        fun apply(n: ISize = -1): None => None
      """)?
    h.assert_eq[String](
      _DefaultValueTestHelper.find_default(prog, "apply")
        as String,
      "-1")

class \nodoc\ _TestDefaultValueStringLiteral is UnitTest
  fun name(): String => "DefaultValue/string-literal"

  fun apply(h: TestHelper) ? =>
    let prog = _ASTTestHelper.extract(h,
      """
      primitive Foo
        fun apply(s: String = "hello"): None => None
      """)?
    h.assert_eq[String](
      _DefaultValueTestHelper.find_default(prog, "apply")
        as String,
      "\"hello\"")

class \nodoc\ _TestDefaultValueBoolLiteral is UnitTest
  fun name(): String => "DefaultValue/bool-literal"

  fun apply(h: TestHelper) ? =>
    let prog = _ASTTestHelper.extract(h,
      """
      primitive Foo
        fun apply(b: Bool = true): None => None
      """)?
    h.assert_eq[String](
      _DefaultValueTestHelper.find_default(prog, "apply")
        as String,
      "true")

class \nodoc\ _TestDefaultValueReference is UnitTest
  fun name(): String => "DefaultValue/reference"

  fun apply(h: TestHelper) ? =>
    let prog = _ASTTestHelper.extract(h,
      """
      primitive Foo
        fun apply(x: (String | None) = None): None => None
      """)?
    h.assert_eq[String](
      _DefaultValueTestHelper.find_default(prog, "apply")
        as String,
      "None")

class \nodoc\ _TestDefaultValueMethodCall is UnitTest
  fun name(): String => "DefaultValue/method-call"

  fun apply(h: TestHelper) ? =>
    let prog = _ASTTestHelper.extract(h,
      """
      primitive Foo
        fun apply(n: ISize = ISize.max_value()): None => None
      """)?
    h.assert_eq[String](
      _DefaultValueTestHelper.find_default(prog, "apply")
        as String,
      "ISize.max_value()")

class \nodoc\ _TestDefaultValueConstructorCall is UnitTest
  fun name(): String => "DefaultValue/constructor-call"

  fun apply(h: TestHelper) ? =>
    let prog = _ASTTestHelper.extract(h,
      """
      primitive Foo
        fun apply(x: I64 = I64(42)): None => None
      """)?
    h.assert_eq[String](
      _DefaultValueTestHelper.find_default(prog, "apply")
        as String,
      "I64(42)")

class \nodoc\ _TestDefaultValueNone is UnitTest
  fun name(): String => "DefaultValue/no-default"

  fun apply(h: TestHelper) ? =>
    let prog = _ASTTestHelper.extract(h,
      """
      primitive Foo
        fun apply(n: USize): None => None
      """)?
    h.assert_is[None](
      _DefaultValueTestHelper.find_default(prog, "apply")
        as None,
      None)
