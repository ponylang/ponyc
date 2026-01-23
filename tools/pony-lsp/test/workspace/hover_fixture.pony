"""
Test fixtures for LSP hover functionality.

This file provides test cases for manual testing of hover information.
It contains various Pony entities (classes, actors, traits, methods, fields)
with docstrings and type annotations to validate hover behavior.

To test hover functionality:
1. Open the lsp/test/workspace directory as a project in your editor
2. Open this file with the Pony language server configured
3. Hover your cursor over various code elements to see hover information

Expected hover behavior:
- Display the declaration signature in a code block
- Include docstrings where present
- Show type information for fields and variables
"""

class HoverTestClass
  """
  A sample class for testing hover.
  """
  let field_name: String
  var mutable_field: U32
  embed embedded_field: Array[String] = Array[String]

  new create(name: String) =>
    field_name = name
    mutable_field = 0

  fun simple_method(): String =>
    """Returns the field name."""
    field_name

  fun method_with_params(x: U32, y: String): Bool =>
    """A method with parameters."""
    x > 0

  fun ref update_field(count: USize) =>
    """Update the mutable field."""
    mutable_field = count.u32()

actor HoverTestActor
  """
  A sample actor for testing hover.
  """
  let _name: String

  new create(name': String) =>
    _name = name'

  be do_something(value: U64) =>
    None

trait HoverTestTrait
  """
  A sample trait for testing hover.
  """
  fun trait_method(): String

interface HoverTestInterface
  """
  A sample interface for testing hover.
  """
  fun interface_method(): None val

primitive HoverTestPrimitive
  """
  A sample primitive for testing hover.
  """
  fun apply(): String => "test"

type HoverTestAlias is (String | U32 | None)

class TypeInferenceDemo
  """
  Demonstrates that type inference works in hover!
  Hover shows inferred types on variable declarations even without explicit annotations.
  """

  fun demo_type_inference(): String =>
    """
    Hover shows inferred types on variable DECLARATIONS!
    Try hovering over the variable names on their declaration lines.

    ACTUAL: Hover shows 'let inferred_bool: Bool' even without explicit type
    This works because the type-checked AST contains inferred type info.
    """
    let inferred_string = "test"       // Hover shows: let inferred_string: String val
    let inferred_bool = true           // Hover shows: let inferred_bool: Bool
    let inferred_array = Array[U32]    // Hover shows: let inferred_array: Array[U32] ref

    inferred_string + inferred_bool.string() + inferred_array.size().string()

class FunctionCallHoverDemo
  """
  Demonstrates that hover works on function calls!
  Hovering over a method name in a call shows its signature and docstring.
  """

  fun demo_function_calls(): String =>
    """
    Try hovering over the method names in the function calls below.
    Hover will show the method signature with parameters and docstring.
    """
    let result1 = this.helper_method("test")
    let result2 = this.method_with_multiple_params(42, "hello", true)
    result1 + result2

  fun helper_method(input: String): String =>
    """A helper method that processes input."""
    input.upper()

  fun method_with_multiple_params(x: U32, y: String, flag: Bool): String =>
    """Method with multiple parameters for testing."""
    y

class ComplexTypesDemo
  """
  Demonstrates that hover works on complex type expressions.
  Union types, tuple types, and intersection types are formatted correctly.
  """

  fun demo_complex_types(): String =>
    """
    Hover over the variable names on their declaration lines to see formatted types.
    Hover shows 'let union_type: (String | U32 | None)' and similar.
    """
    let union_type: (String | U32 | None) = "test"
    let tuple_type: (String, U32, Bool) = ("test", U32(42), true)
    union_type.string() + tuple_type._1.string()

// ========== Examples of Current Limitations ==========

class LimitationExamples
  """
  This class demonstrates hover limitations - cases where hover doesn't
  currently provide information but ideally should.
  """

  // Limitation 1: Variable usage doesn't show type
  fun demo_variable_usage() =>
    """
    Try hovering over the variable names when they're USED (not declared).
    EXPECTED: Should show variable type
    ACTUAL: No hover information (only works on the declaration line)
    """
    let count: U32 = 100
    let name: String = "example"

    // Hover over 'count' or 'name' below - no info shown
    let doubled = count * 2
    let upper_name = name.upper()
    let sum = count + doubled

  // Limitation 2: Primitive type documentation
  fun demo_primitive_types(): USize =>
    """
    Try hovering over primitive numeric types vs classes.
    Classes (String, Array): Show full documentation with docstrings
    Primitives (U32, I64, etc.): Show minimal info, just 'primitive U32'

    This is because primitives in the stdlib may lack docstrings or they
    aren't being extracted properly from the builtin package.
    """
    let text: String = ""           // Hover shows full String documentation
    let numbers: Array[U32] = []    // Hover shows full Array documentation
    let value: U32 = 0              // Hover shows just: primitive U32
    text.size() + numbers.size() + value.usize()

  // Limitation 3: Receiver capabilities not shown in signatures
  fun box boxed_method(): String =>
    """
    A boxed method - receiver capability is 'box'.

    EXPECTED: Hover should show 'fun box boxed_method(): String'
    ACTUAL: Hover shows 'fun boxed_method(): String' (missing 'box')

    The receiver capability indicates how 'this' can be accessed in the method.
    """
    "box"

  fun val valued_method(): String =>
    """
    A val method - receiver capability is 'val'.

    EXPECTED: Should show 'fun val valued_method(): String'
    ACTUAL: Shows 'fun valued_method(): String' (missing 'val')
    """
    "val"

  fun ref mutable_method() =>
    """
    A ref method - receiver capability is 'ref'.

    EXPECTED: Should show 'fun ref mutable_method()'
    ACTUAL: Shows 'fun mutable_method()' (missing 'ref')
    """
    None
