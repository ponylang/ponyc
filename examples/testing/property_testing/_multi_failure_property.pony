use "pony_test"

class \nodoc\ iso _MultiFailureProperty is Property[U8]
  """
  A property with two deliberate bugs: fails on even numbers and
  on multiples of 7. With max_distinct_failures = 5, the test run
  reports both in a single pass.
  """
  fun name(): String => "multi_failure/two_bugs"

  fun gen(): Generator[U8] => Generators.u8(0, 100)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 200,
      max_distinct_failures' = 5, regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) ? =>
    if (sample % 2) == 0 then error end
    if (sample % 7) == 0 then error end
