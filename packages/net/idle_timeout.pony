use "constrained_types"

primitive IdleTimeoutValidator is Validator[U64]
  """
  Validates that an idle timeout duration is within the allowed range.

  The minimum value is 1 millisecond. The maximum value is
  18,446,744,073,709 milliseconds (~213,503 days) — the largest value
  that can be converted to nanoseconds without overflowing U64.

  Used by `MakeIdleTimeout` to construct `IdleTimeout` values.
  """
  fun apply(value: U64): ValidationResult =>
    if value == 0 then
      recover val
        ValidationFailure(
          "idle timeout must be greater than zero")
      end
    elseif value > _max_millis() then
      recover val
        ValidationFailure(
          "idle timeout must be at most " +
            _max_millis().string() +
            " milliseconds")
      end
    else
      ValidationSuccess
    end

  fun _max_millis(): U64 =>
    """
    The maximum idle timeout in milliseconds. Values above this would
    overflow U64 when converted to nanoseconds internally.
    """
    U64.max_value() / 1_000_000

type IdleTimeout is Constrained[U64, IdleTimeoutValidator]
  """
  A validated idle timeout duration in milliseconds. The allowed range is
  1 to 18,446,744,073,709 milliseconds (~213,503 days). The upper bound
  ensures the value can be safely converted to nanoseconds without
  overflowing U64.

  Construct with `MakeIdleTimeout(milliseconds)`, which returns
  `(IdleTimeout | ValidationFailure)`. Pass to `idle_timeout()` to set the
  timeout, or pass `None` to disable it.
  """

type MakeIdleTimeout is MakeConstrained[U64, IdleTimeoutValidator]
  """
  Factory for `IdleTimeout` values. Returns `(IdleTimeout | ValidationFailure)`.
  """
