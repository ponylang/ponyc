class HoverTestClass
  """
  A sample class for testing hover.
  """
  let field_name: String
  var mutable_field: U32
  embed embedded_field: Array[String] =
    Array[String]

  new create(name: String) =>
    field_name = name
    mutable_field = 0

  fun simple_method(): String =>
    """
    Returns the field name.
    """
    field_name

  fun method_with_params(
    x: U32, y: String): Bool
  =>
    """
    A method with parameters.
    """
    x > 0

  fun ref update_field(count: USize) =>
    """
    Update the mutable field.
    """
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
    """
    A method defined by a trait.
    """

interface HoverTestInterface
  """
  A sample interface for testing hover.
  """

  fun interface_method(): None val
    """
    A method defined by an interface.
    """

primitive HoverTestPrimitive
  """
  A sample primitive for testing hover.
  """
  fun apply(): String => "test"

type HoverTestAlias is (String | U32 | None)

class TypeInferenceDemo
  """
  Demonstrates that type inference works in hover.
  """

  fun demo_type_inference(): String =>
    """
    Hover shows inferred types on variable
    DECLARATIONS.
    """
    let inferred_string = "test"
    let inferred_bool = true
    let inferred_array = Array[U32]

    inferred_string +
      inferred_bool.string() +
      inferred_array.size().string()

class FunctionCallHoverDemo
  """
  Demonstrates that hover works on function calls.
  """

  fun demo_function_calls(): String =>
    """
    Try hovering over the method names in the
    function calls below.
    """
    let result1 =
      this.helper_method("test")
    let result2 =
      this.method_with_multiple_params(
        42, "hello", true)
    result1 + result2

  fun helper_method(
    input: String): String
  =>
    """
    A helper method that processes input.
    """
    input.upper()

  fun method_with_multiple_params(
    x: U32, y: String, flag: Bool): String
  =>
    """
    Method with multiple parameters for testing.
    """
    y

class ComplexTypesDemo
  """
  Demonstrates that hover works on complex type
  expressions.
  """

  fun demo_complex_types(): String =>
    """
    Hover over the variable names on their
    declaration lines to see formatted types.
    """
    let union_type:
      (String | U32 | None) = "test"
    let tuple_type: (String, U32, Bool) =
      ("test", U32(42), true)
    union_type.string() +
      tuple_type._1.string()

class GenericsDemo[T: Any val]
  """
  Demonstrates hover on generic classes with type
  parameters.
  """
  let _value: T

  new create(value: T) =>
    _value = value

  fun get(): T =>
    """
    Returns the stored value.
    """
    _value

  fun with_generic_param[U: Any val](
    other: U): (T, U)
  =>
    """
    A generic method with its own type parameter.
    """
    (_value, other)

class GenericPair[A: Any val, B: Any val]
  """
  Demonstrates multiple type parameters.
  """
  let first: A
  let second: B

  new create(a: A, b: B) =>
    first = a
    second = b

  fun get_first(): A =>
    """
    Returns the first element.
    """
    first

  fun get_second(): B =>
    """
    Returns the second element.
    """
    second

trait MyComparable[T: MyComparable[T] box]
  """
  A trait demonstrating recursive type constraints.
  Used for testing hover on constrained generics.
  """
  fun compare(that: box->T): I32

class GenericContainer[T: Stringable val]
  """
  Demonstrates type constraints on generic
  parameters.
  """
  let _items: Array[T]

  new create() =>
    _items = Array[T]

  fun ref add(item: T) =>
    """
    Add an item to the container.
    """
    _items.push(item)

  fun get_strings(): Array[String] =>
    """
    Returns string representations of all items.
    """
    let result = Array[String]
    for item in _items.values() do
      result.push(item.string())
    end
    result

actor GenericActor[T: Any val]
  """
  Demonstrates generic actors.
  """
  var _state: T

  new create(initial: T) =>
    _state = initial

  be update(new_state: T) =>
    """
    Updates the actor's state.
    """
    _state = new_state

  be process[U: Any val](data: U) =>
    """
    A generic behavior with its own type parameter.
    """
    None

primitive GenericHelper
  """
  Demonstrates generic methods in primitives.
  """

  fun identity[T: Any val](value: T): T =>
    """
    A generic identity function.
    """
    value

  fun create_pair[A: Any val, B: Any val](
    a: A, b: B): (A, B)
  =>
    """
    Creates a tuple from two values of different
    types.
    """
    (a, b)

  fun apply[T: Any val](): Array[T] =>
    """
    Creates an empty array of any type.
    """
    Array[T]

class GenericUsageDemo
  """
  Demonstrates hover on instantiated generic types.
  """

  fun demo_generic_instantiation() =>
    """
    Try hovering over the variable names.
    """
    let pair: GenericPair[String, U32] =
      GenericPair[String, U32]("hello", 42)
    let container:
      GenericContainer[String] =
        GenericContainer[String]
    let numbers: GenericsDemo[U64] =
      GenericsDemo[U64](100)

    // Hover over method calls with generic types
    let first = pair.get_first()
    let result =
      numbers.with_generic_param[String]("test")
    None

class ReceiverCapabilityDemo
  """
  Demonstrates that hover displays receiver
  capabilities in method signatures.
  """

  fun box boxed_method(): String =>
    """
    A boxed method - receiver capability is 'box'.
    """
    "box"

  fun val valued_method(): String =>
    """
    A val method - receiver capability is 'val'.
    """
    "val"

  fun ref mutable_method() =>
    """
    A ref method - receiver capability is 'ref'.
    """
    None
