use "constrained_types"

primitive TimerDurationValidator is Validator[U64]
  """
  Validates that a timer duration is within the allowed range.

  The minimum value is 1 millisecond. The maximum value is
  18,446,744,073,709 milliseconds (~213,503 days) — the largest value
  that can be converted to nanoseconds without overflowing U64.

  Used by `MakeTimerDuration` to construct `TimerDuration` values.
  """
  fun apply(value: U64): ValidationResult =>
    if value == 0 then
      recover val
        ValidationFailure(
          "timer duration must be greater than zero")
      end
    elseif value > _max_millis() then
      recover val
        ValidationFailure(
          "timer duration must be at most " +
            _max_millis().string() +
            " milliseconds")
      end
    else
      ValidationSuccess
    end

  fun _max_millis(): U64 =>
    """
    The maximum timer duration in milliseconds. Values above this would
    overflow U64 when converted to nanoseconds internally.
    """
    U64.max_value() / 1_000_000

type TimerDuration is Constrained[U64, TimerDurationValidator]
  """
  A validated timer duration in milliseconds. The allowed range is
  1 to 18,446,744,073,709 milliseconds (~213,503 days). The upper bound
  ensures the value can be safely converted to nanoseconds without
  overflowing U64.

  Construct with `MakeTimerDuration(milliseconds)`, which returns
  `(TimerDuration | ValidationFailure)`. Pass to `set_timer()` to create
  a one-shot timer.
  """

type MakeTimerDuration is MakeConstrained[U64, TimerDurationValidator]
  """
  Factory for `TimerDuration` values. Returns
  `(TimerDuration | ValidationFailure)`.
  """
