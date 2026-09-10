use "constrained_types"

primitive MaxSpawnValidator is Validator[U32]
  """
  Validates that a max spawn value is at least 1.

  A limit of 0 is nonsensical — it would prevent any connections from being
  accepted. Used by `MakeMaxSpawn` to construct `MaxSpawn` values.
  """
  fun apply(value: U32): ValidationResult =>
    if value == 0 then
      recover val
        ValidationFailure("max spawn must be greater than zero")
      end
    else
      ValidationSuccess
    end

type MaxSpawn is Constrained[U32, MaxSpawnValidator]
  """
  A validated maximum number of concurrent connections. The value must be
  at least 1.

  Construct with `MakeMaxSpawn(count)`, which returns
  `(MaxSpawn | ValidationFailure)`. Pass to `TCPListener` via the `limit`
  parameter, or pass `None` to disable the connection limit.
  """

type MakeMaxSpawn is MakeConstrained[U32, MaxSpawnValidator]
  """
  Factory for `MaxSpawn` values. Returns `(MaxSpawn | ValidationFailure)`.
  """

primitive DefaultMaxSpawn
  """
  Returns a `MaxSpawn` with the default connection limit of 100,000.
  """
  fun apply(): MaxSpawn =>
    match \exhaustive\ MakeMaxSpawn(100_000)
    | let m: MaxSpawn => m
    | let _: ValidationFailure =>
      // Known unreachable: 100,000 is always valid.
      _Unreachable()
      apply()
    end
