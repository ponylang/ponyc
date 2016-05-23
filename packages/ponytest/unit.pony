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

  fun ref apply(h: TestHelper) ?
    """
    Run the test.
    Raising an error is interpreted as a test failure.
    """

  fun ref timed_out(h: TestHelper) =>
    """
    Tear down a possibly hanging test.
    Called when the timeout specified by to long_test() expires.
    There is no need for this function to call complete(false).
    tear_down() will still be called after this completes.
    The default is to do nothing.
    """
    None

  fun ref tear_down(h: TestHelper) =>
    """
    Tidy up after the test has completed.
    Called for each run test, whether that test passed, succeeded or timed out.
    The default is to do nothing.
    """
    None

  fun label() : String =>
    """
    Report the test label, returning an empty string for none.
    It can be later use to filter tests which we want to run, by labels.
    """
    ""
