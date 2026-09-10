use "constrained_types"

primitive ConnectionTimeoutValidator is Validator[U64]
  """
  Validates that a connection timeout duration is within the allowed range.

  The minimum value is 1 millisecond. The maximum value is
  18,446,744,073,709 milliseconds (~213,503 days) — the largest value
  that can be converted to nanoseconds without overflowing U64.

  Used by `MakeConnectionTimeout` to construct `ConnectionTimeout` values.
  """
  fun apply(value: U64): ValidationResult =>
    if value == 0 then
      recover val
        ValidationFailure(
          "connection timeout must be greater than zero")
      end
    elseif value > _max_millis() then
      recover val
        ValidationFailure(
          "connection timeout must be at most " +
            _max_millis().string() +
            " milliseconds")
      end
    else
      ValidationSuccess
    end

  fun _max_millis(): U64 =>
    """
    The maximum connection timeout in milliseconds. Values above this would
    overflow U64 when converted to nanoseconds internally.
    """
    U64.max_value() / 1_000_000

type ConnectionTimeout is Constrained[U64, ConnectionTimeoutValidator]
  """
  A validated connection timeout duration in milliseconds. The allowed range is
  1 to 18,446,744,073,709 milliseconds (~213,503 days). The upper bound
  ensures the value can be safely converted to nanoseconds without
  overflowing U64.

  Construct with `MakeConnectionTimeout(milliseconds)`, which returns
  `(ConnectionTimeout | ValidationFailure)`. Pass to the `client` or
  `ssl_client` constructor's `connection_timeout` parameter, or pass `None`
  to disable it (the default).
  """

type MakeConnectionTimeout is MakeConstrained[U64, ConnectionTimeoutValidator]
  """
  Factory for `ConnectionTimeout` values. Returns
  `(ConnectionTimeout | ValidationFailure)`.
  """
