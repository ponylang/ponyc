"""
# PonyTest package

The PonyTest package provides a unit testing framework. It is designed to be as
simple as possible to use, both for the unit test writer and the user running
the tests.

Each unit test is a class, with a single test function. By default all tests
run concurrently.

Each test run is provided with a helper object. This provides logging and
assertion functions. By default log messages are only shown for tests that
fail.

When any assertion function fails the test is counted as a fail. However, tests
can also indicate failure by return false or throwing errors from the test
function.

## Example program

To use PonyTest simply write a class for each test and a TestList type that
tells the PonyTest object about the tests. Typically the TestList will be Main
for the package.

The following is a complete program with 2 trivial tests.

```
use "ponytest"

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_TestAdd)
    test(_TestSub)

class _TestAdd iso is UnitTest
  fun name():String => "addition"

  fun apply(h: TestHelper): TestResult =>
    h.assert_eq[U32](4, 2 + 2)
    true

class _TestSub iso is UnitTest
  fun name():String => "subtraction"

  fun apply(h: TestHelper): TestResult =>
    h.assert_eq[U32](2, 4 - 2)
    true
```

The make() constructor is not needed for this example. However, it allows for
easy aggregation of tests (see below) so it is recommended that all test Mains
provide it.

Main.create() is called only for program invocations on the current pacakge.
Main.make() is called during aggregation. If so desired extra code can be added
to either of these constructors to perform additional tasks.

## Test names

Tests are identified by names, which are used when printing test results and on
the command line to select which tests to run. These names are independent of
the names of the test classes in the Pony source code.

Arbitrary strings can be used for these names, but for large projects it is
strongly recommended to use a hierarchical naming scheme to make it easier to
select groups of tests.

## Aggregation

Often it is desirable to run a collection of unit tests from multiple different
source files. For example, if several packages within a bundle each have their
own unit tests it may be useful to run all tests for the bundle together.

This can be achieved by writing an aggregate test list class, which calls the
list function for each package. The following is an example that aggregates the
tests from packages `foo` and `bar`.

```
use "ponytest"
use foo = "foo"
use bar = "bar"

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    foo.Main.make().tests(test)
    bar.Main.make().tests(test)
```

Aggregate test classes may themselves be aggregated. Every test list class may
contain any combination of its own tests and aggregated lists.

## Long tests

Simple tests run within a single function. When that function exits, either
returning a result or throwing an error, the test is complete. This is not
viable for tests that need to use actors.

Long tests allow for delayed completion. Any test can return LongTest from its
test function, indicating that the test needs to keep running. When the test is
finally complete it calls the complete() function on its TestHelper.

The complete() function takes a Bool parameter to specify whether the test was
a success. If any asserts fail then the test will be considered a failure
regardless of the value of this parameter. However, complete() must still be
called.

Tests that do not return LongTest can still call complete(), but it is ignored.

## Exclusion groups

By default all tests are run concurrently. This may be a problem for some
tests, eg if they manipulate an external file or use a system resource. To fix
this issue any number of tests may be put into an exclusion group.

No tests that are in the same exclusion group will be run concurrently.

Exclusion groups are identified by name, arbitrary strings may be used.
Multiple exclusion groups may be used and tests in different groups may run
concurrently. Tests that do not specify an exclusion group may be run
concurrently with any other tests.

The command line option "--sequential" prevents any tests from running
concurrently, regardless of exclusion groups. This is intended for debugging
rather than standard use.
"""

use "collections"
use "options"
use "term"


actor PonyTest
  """
  Main test framework actor that organises tests, collates information and
  prints results.
  """

  let _groups: Map[String, _Group] = Map[String, _Group]
  let _records: Array[_TestRecord] = Array[_TestRecord]
  let _env: Env
  var _do_nothing: Bool = false
  var _filter: String = ""
  var _verbose: Bool = false
  var _sequential: Bool = false
  var _started: U64 = 0
  var _finished: U64 = 0
  var _any_found: Bool = false
  var _all_started: Bool = false

  new create(env: Env, list: TestList tag) =>
    """
    Create a PonyTest object and use it to run the tests from the given
    TestList
    """
    _env = env
    _process_opts()
    _groups("") = _SimultaneousGroup
    list.tests(this)
    _run()

  be apply(test: UnitTest iso) =>
    """
    Run the given test, subject to our filter and options.
    """
    if _do_nothing then
      return
    end

    var name = test.name()

    // Ignore any tests that don't satisfy our filter
    if not name.at(_filter, 0) then
      return
    end

    _any_found = true
    var index = _records.size()
    _records.push(_TestRecord(_env, name))

    var group = _find_group(test.exclusion_group())
    group(TestHelper._create(this, index, consume test, group, _verbose))

  fun ref _find_group(group_name: String): _Group =>
    """
    Find the group to use for the given group name, subject to the
    --sequential flag.
    """
    var name = group_name

    if _sequential then
      // Use the same group for all tests.
      name = "all"
    end

    try
      _groups(name)
    else
      // Group doesn't exist yet, make it.
      // We only need one simultanous group, which we've already made. All new
      // groups are exclusive.
      var g = _ExclusiveGroup
      _groups(name) = g
      g
    end

  be _test_started(id: U64) =>
    """
    A test has started running, update status info.
    The id parameter is the test identifier handed out when we created the test
    helper.
    """
    _started = _started + 1
    _env.out.print(_started.string() + " test" + _plural(_started) +
      " started, " + _finished.string() + " complete")

  be _test_complete(id: U64, pass: Bool, log: Array[String] val) =>
    """
    A test has completed, restore its result and update our status info.
    The id parameter is the test identifier handed out when we created the test
    helper.
    """
    try
      _records(id)._result(pass, log)
    end

    _finished = _finished + 1
    _env.out.print(_started.string() + " test" + _plural(_started) +
      " started, " + _finished.string() + " complete")

    if _all_started and (_finished == _records.size()) then
      // All tests have completed
      _print_report()
    end

  be _run() =>
    """
    We've been given all the tests to run, now run them.
    """
    if _do_nothing then
      return
    end

    if not _any_found then
      // No tests matched our filter, print special message.
      _env.out.print("No tests found")
      return
    end

    _all_started = true
    if _finished == _records.size() then
      // All tests have completed
      _print_report()
    end

  fun ref _process_opts() =>
    """
    Process our command line options.
    All command line arguments given must be recognised and make sense.
    State for specified options is stored in object fields.
    """
    // TODO: Options doesn't currently fully work for printing usage etc, this
    // may need reworking when it's sorted out
    var opts = Options(_env)

    opts/*.usage_text(
      """
      ponytest [OPTIONS]

      Options:
      """)*/
      .add("sequential", "s"/*, "Tests run sequentially."*/, None)
      .add("verbose", "v"/*, "Show verbose logs."*/, None)
      .add("filter", "f"
        /*,"Only run the tests whose names start with the given prefix."*/,
        StringArgument)

    for option in opts do
      match option
      | ("sequential", None) => _sequential = true
      | ("verbose", None) => _verbose = true
      | ("filter", var arg: String) => _filter = arg
      | let failure: ParseError =>
        //opts.usage()
        failure.report(_env.out)
        _do_nothing = true
      end
    end

  fun _print_report() =>
    """
    The tests are all complete, print out the results.
    """
    var pass_count: U64 = 0
    var fail_count: U64 = 0

    // First we print the result summary for each test, in the order that they
    // were given to us.
    for rec in _records.values() do
      if rec._report(_verbose) then
        pass_count = pass_count + 1
      else
        fail_count = fail_count + 1
      end
    end

    // Next we print the pass / fail stats.
    _env.out.print("----")
    _env.out.print("---- " + _records.size().string() + " test" +
      _plural(_records.size()) + " ran.")
    _env.out.print(ANSI.bright_green() + "---- Passed: " +
      pass_count.string() + ANSI.reset())

    if fail_count == 0 then
      // Success, nothing failed.
      return
    end

    // Not everything passed.
    _env.out.print(ANSI.bright_red() + "**** FAILED: " + fail_count.string() +
      " test" + _plural(fail_count) + ", listed below:" + ANSI.reset())

    // Finally print our list of failed tests.
    for rec in _records.values() do
      rec._list_failed()
    end

    _env.exitcode(-1)

  fun _plural(n: U64): String =>
    """
    Return a "s" or an empty string depending on whether the given number is 1.
    For use when printing possibly plural words, eg "test" or "tests".
    """
    if n == 1 then "" else "s" end
