actor PassReporter
  """
  Reporter for sub-PonyTest runs expected to pass.

  Completes the given `TestHelper` with success when all tests
  in the sub-run pass. If any test fails, does nothing — the
  outer test's timeout handles the failure case.
  """
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  be test_started(name: String) => None

  be test_complete(result: TestResult val) => None

  be testing_complete(results: Array[TestResult val] val) =>
    var all_passed = true
    for r in results.values() do
      if not r.passed then all_passed = false; break end
    end
    if all_passed then _h.complete(true) end
