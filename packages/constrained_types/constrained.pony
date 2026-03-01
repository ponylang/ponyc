type ValidationResult is (ValidationSuccess | ValidationFailure)

primitive ValidationSuccess

class val ValidationFailure
  """
  Collection of validation errors.
  """
  let _errors: Array[String val] = _errors.create()

  new create(e: (String val | None) = None) =>
    match e
    | let s: String val => _errors.push(s)
    end

  fun ref apply(e: String val) =>
    """
    Add an error to the failure.
    """
    _errors.push(e)

  fun errors(): this->Array[String val] =>
    """
    Get list of validation errors.
    """
    _errors

interface val Validator[T]
  """
  Interface validators must implement.

  We strongly suggest you use a `primitive` for your `Validator` as validators
  are required to be stateless.
  """
  new val create()
  fun apply(i: T): ValidationResult
  """
  Takes an instance and returns either `ValidationSuccess` if it meets the
  constraint criteria or `ValidationFailure` if it doesn't.
  """

class val Constrained[T: Any val, F: Validator[T]]
  """
  Wrapper class for a constrained type.
  """
  let _value: T val

  new val _create(value: T val) =>
    """
    Private constructor that guarantees that `Constrained` can only be created
    by the `MakeConstrained` primitive in this package.
    """
    _value = value

  fun val apply(): T val =>
    """
    Unwraps and allows access to the constrained object.
    """
    _value

primitive MakeConstrained[T: Any val, F: Validator[T] val]
  """
  Builder of `Constrained` instances.
  """
  fun apply(value: T): (Constrained[T, F] | ValidationFailure) =>
    match \exhaustive\ F(value)
    | ValidationSuccess => Constrained[T, F]._create(value)
    | let e: ValidationFailure => e
    end
