class val TestResult
  """
  The structured outcome of a single test: its name, whether it passed,
  and the log messages it produced.
  """
  let name: String
  let passed: Bool
  let log: Array[String] val

  new val create(name': String, passed': Bool, log': Array[String] val) =>
    name = name'
    passed = passed'
    log = log'

interface tag TestReporter
  """
  Receives structured test-result events from PonyTest.

  Implement this interface on an actor to receive programmatic
  test results. Pass the actor to `PonyTest.create` as the
  `reporter` parameter.

  Three events arrive during a test run:

  - `test_started` when a test begins executing.
  - `test_complete` when a test finishes, carrying the full result.
    Results arrive in completion order.
  - `testing_complete` after all tests finish, carrying every result
    in registration order.
  """
  be test_started(name: String)
    """
    Called when a test begins executing.
    """

  be test_complete(result: TestResult val)
    """
    Called when a test finishes. Results arrive in completion order.
    """

  be testing_complete(results: Array[TestResult val] val)
    """
    Called after all tests finish. The array is in registration order.
    """
