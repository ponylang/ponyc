trait _Generics[T: _Generics[T] box]
  """
  A trait demonstrating recursive type constraints.
  Used for testing hover on constrained generics.
  """
  fun compare(that: box->T): I32

class _GenericsDemo[T: Any val]
  """
  Demonstrates hover on generic classes with type parameters.
  Hover over the class name shows the full signature with type parameters.
  """
  let _value: T

  new create(value: T) =>
    _value = value

  fun get(): T =>
    """
    Returns the stored value.
    """
    _value

  fun with_generic_param[U: Any val](other: U): (T, U) =>
    """
    A generic method with its own type parameter.
    Hover shows both the class type parameter T and method type parameter U.
    """
    (_value, other)

class _GenericPair[A: Any val, B: Any val]
  """
  Demonstrates multiple type parameters.
  Hover shows 'class GenericPair[A: Any val, B: Any val]'
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

class _GenericContainer[T: Stringable val]
  """
  Demonstrates type constraints on generic parameters.
  Hover shows 'class GenericContainer[T: Stringable val]'
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
    Works because T is constrained to Stringable.
    """
    let result = Array[String]
    for item in _items.values() do
      result.push(item.string())
    end
    result

actor _GenericActor[T: Any val]
  """
  Demonstrates generic actors.
  Hover shows 'actor GenericActor[T: Any val]'
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
    Demonstrates that behaviors can be generic too.
    """
    None

primitive _GenericHelper
  """
  Demonstrates generic methods in primitives.
  """
  fun identity[T: Any val](value: T): T =>
    """
    A generic identity function.
    Hover shows 'fun identity[T: Any val](value: T): T'
    """
    value

  fun create_pair[A: Any val, B: Any val](a: A, b: B): (A, B) =>
    """
    Creates a tuple from two values of different types.
    """
    (a, b)

  fun apply[T: Any val](): Array[T] =>
    """
    Creates an empty array of any type.
    Hover shows the generic return type Array[T].
    """
    Array[T]

class _GenericUsageDemo
  """
  Demonstrates hover on instantiated generic types.
  """
  fun demo_generic_instantiation() =>
    """
    Try hovering over the variable names (pair, container, numbers).
    Hover shows the variable declaration with fully instantiated generic types
    like 'let pair: GenericPair[String val, U32 val]'.

    Note: Hovering on the type names themselves (GenericPair, GenericsDemo) will
    follow to the class definition and show the type parameters instead.
    """
    let pair: _GenericPair[String, U32] = _GenericPair[String, U32]("hello", 42)
    let container: _GenericContainer[String] = _GenericContainer[String]
    let numbers: _GenericsDemo[U64] = _GenericsDemo[U64](100)

    // Hover over method calls with generic types

    // Hover shows: let first: String val
    let first = pair.get_first()

    // Hover shows: let result: (U64 val, String val)
    let result = numbers.with_generic_param[String]("test")

    None
