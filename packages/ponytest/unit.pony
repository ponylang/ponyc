primitive LongTest
type TestResult is (Bool | LongTest)


trait UnitTest
  """
  Each unit test class must provide this trait. Simple tests only need to
  define the name() and apply() functions. The remaining functions specify
  additional test options.
  """

  fun name(): String
    """
    Report the test name, which is used when printing test results and on the
    command line to select tests to run.
    """

  fun exclusion_group(): String =>
    """
    Report the test exclusion group, returning an empty string for none.
    The default body returns an empty string.
    """
    ""

  fun ref apply(t: TestHelper): TestResult ?
    """
    Run the test.
    Return values:
    * true - test passed.
    * false - test failed.
    * LongTest - test needs to run for longer. See package doc string.
    * error - test failed.
    """
