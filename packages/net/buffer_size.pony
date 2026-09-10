use "constrained_types"

primitive BufferSizeValidator is Validator[USize]
  """
  Validates that a buffer-until value is at least 1.

  A buffer-until of 0 is meaningless — use `Streaming` to indicate "deliver all
  available data." Used by `MakeBufferSize` to construct `BufferSize` values.
  """
  fun apply(value: USize): ValidationResult =>
    if value == 0 then
      recover val
        ValidationFailure("buffer size must be greater than zero")
      end
    else
      ValidationSuccess
    end

type BufferSize is Constrained[USize, BufferSizeValidator]
  """
  A validated buffer-until value in bytes. The value must be at least 1.

  Construct with `MakeBufferSize(bytes)`, which returns
  `(BufferSize | ValidationFailure)`. Pass to `TCPConnection.buffer_until()`.
  Use `Streaming` instead of `BufferSize` to indicate "deliver all available
  data."
  """

type MakeBufferSize is MakeConstrained[USize, BufferSizeValidator]
  """
  Factory for `BufferSize` values. Returns `(BufferSize | ValidationFailure)`.
  """

primitive Streaming
  """
  Pass to `TCPConnection.buffer_until()` to indicate streaming mode: deliver
  all available data as it arrives, with no buffering threshold.
  """
