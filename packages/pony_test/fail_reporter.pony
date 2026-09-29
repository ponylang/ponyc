actor FailReporter
  """
  Reporter for sub-PonyTest runs expected to fail.

  Completes the given `TestHelper` with success when any test
  in the sub-run fails. If all tests pass, does nothing — the
  outer test's timeout handles the unexpected-pass case.
  """
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  be test_started(name: String) => None

  be test_complete(result: TestResult val) => None

  be testing_complete(results: Array[TestResult val] val) =>
    for r in results.values() do
      if not r.passed then
        _h.complete(true)
        return
      end
    end
