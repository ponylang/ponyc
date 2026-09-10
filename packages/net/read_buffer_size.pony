use "constrained_types"

primitive ReadBufferSizeValidator is Validator[USize]
  """
  Validates that a read buffer size is at least 1.

  A buffer of 0 bytes cannot hold any data and would stall the read loop.
  Used by `MakeReadBufferSize` to construct `ReadBufferSize` values.
  """
  fun apply(value: USize): ValidationResult =>
    if value == 0 then
      recover val
        ValidationFailure("read buffer size must be greater than zero")
      end
    else
      ValidationSuccess
    end

type ReadBufferSize is Constrained[USize, ReadBufferSizeValidator]
  """
  A validated read buffer size in bytes. The value must be at least 1.

  Construct with `MakeReadBufferSize(bytes)`, which returns
  `(ReadBufferSize | ValidationFailure)`. Pass to `TCPConnection`
  constructors via the `read_buffer_size` parameter, or to
  `set_read_buffer_minimum()` and `resize_read_buffer()`.
  """

type MakeReadBufferSize is MakeConstrained[USize, ReadBufferSizeValidator]
  """
  Factory for `ReadBufferSize` values. Returns
  `(ReadBufferSize | ValidationFailure)`.
  """

primitive DefaultReadBufferSize
  """
  Returns a `ReadBufferSize` with the default buffer size of 16384 bytes
  (16KB).
  """
  fun apply(): ReadBufferSize =>
    match \exhaustive\ MakeReadBufferSize(16384)
    | let r: ReadBufferSize => r
    | let _: ValidationFailure =>
      // Known unreachable: 16384 is always valid.
      _Unreachable()
      apply()
    end
