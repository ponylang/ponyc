use "pony_test"

class _HealthCheckProperty is Property[U8]
  """
  A property with a narrow filter and tightened health check thresholds.

  The filter accepts only multiples of 50, rejecting about 98% of
  candidates. With `max_filter_discard_ratio` set to 5.0, the runner
  logs a warning because the actual ratio exceeds that threshold.

  `max_sample_nanos` is set to 1 nanosecond — any real execution exceeds
  that — so the runner also logs a slow-sample warning.

  Run with `--verbose` to see the WARNING lines in the output.
  """
  fun name(): String => "health_check/narrow_filter"

  fun params(): PropertyParams =>
    PropertyParams(where
      num_samples' = 20,
      max_filter_discard_ratio' = 5.0,
      max_sample_nanos' = 1)

  fun gen(): Generator[U8] =>
    Generators.u8(0, 255)
      .filter({(u: U8): (U8^, Bool) => (u, (u % 50) == 0) })

  fun ref property(arg1: U8, h: TestHelper) =>
    h.assert_true((arg1 % 50) == 0)
