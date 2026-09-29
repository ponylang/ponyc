## Add pluggable test result reporting

PonyTest can now send structured test results to a custom reporter instead of printing to stdout. Pass any actor that implements `TestReporter` as the third argument to `PonyTest.create`:

```pony
use "pony_test"

actor JsonReporter
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  be test_started(name: String) =>
    _out.print("{\"event\": \"started\", \"test\": \"" + name + "\"}")

  be test_complete(result: TestResult val) =>
    let status = if result.passed then "pass" else "fail" end
    _out.print("{\"event\": \"complete\", \"test\": \"" + result.name +
      "\", \"status\": \"" + status + "\"}")

  be testing_complete(results: Array[TestResult val] val) =>
    _out.print("{\"event\": \"done\", \"total\": " +
      results.size().string() + "}")

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this, JsonReporter(env.out))

  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_MyTest)
```

`TestResult` carries the test name, pass/fail status, and log messages. When no reporter is supplied, PonyTest uses its existing console output format.

Two built-in reporters simplify meta-testing (running a sub-PonyTest within a test): `PassReporter(h)` completes the outer `TestHelper` when all sub-tests pass, and `FailReporter(h)` completes it when any sub-test fails.
