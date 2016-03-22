"""
# PonyTest package

The PonyTest package provides a unit testing framework. It is designed to be as
simple as possible to use, both for the unit test writer and the user running
the tests.

To help simplify test writing and distribution this package depends on as few
other packages as possible. Currently the required packages are:

  * builtin
  * time
  * collections

Each unit test is a class, with a single test function. By default all tests
run concurrently.

Each test run is provided with a helper object. This provides logging and
assertion functions. By default log messages are only shown for tests that
fail.

When any assertion function fails the test is counted as a fail. However, tests
can also indicate failure by raising an error in the test function.

## Example program

To use PonyTest simply write a class for each test and a TestList type that
tells the PonyTest object about the tests. Typically the TestList will be Main
for the package.

The following is a complete program with 2 trivial tests.

```pony
use "ponytest"

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_TestAdd)
    test(_TestSub)

class iso _TestAdd is UnitTest
  fun name():String => "addition"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](4, 2 + 2)

class iso _TestSub is UnitTest
  fun name():String => "subtraction"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](2, 4 - 2)
```

The make() constructor is not needed for this example. However, it allows for
easy aggregation of tests (see below) so it is recommended that all test Mains
provide it.

Main.create() is called only for program invocations on the current package.
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

```pony
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
returning or raising an error, the test is complete. This is not viable for
tests that need to use actors.

Long tests allow for delayed completion. Any test can call long_test() on its
TestHelper to indicate that it needs to keep running. When the test is finally
complete it calls complete() on its TestHelper.

The complete() function takes a Bool parameter to specify whether the test was
a success. If any asserts fail then the test will be considered a failure
regardless of the value of this parameter. However, complete() must still be
called.

Since failing tests may hang, a timeout must be specified for each long test.
When the test function exits a timer is started with the specified timeout. If
this timer fires before complete() is called the test is marked as a failure
and the timeout is reported.

On a timeout the timed_out() function is called on the unit test object. This
should perform whatever test specific tidy up is required to allow the program
to exit. There is no need to call complete() if a timeout occurs, although it
is not an error to do so.

Note that the timeout is only relevant when a test hangs and would otherwise
prevent the test program from completing. Setting a very long timeout on tests
that should not be able to hang is perfectly acceptable and will not make the
test take any longer if successful.

Timeouts should not be used as the standard method of detecting if a test has
failed.

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

## Tear down

Each unit test object may define a tear_down() function. This is called after
the test has finished to allow tearing down of any complex environment that had
to be set up for the test.

The tear_down() function is called for each test regardless of whether it
passed or failed. If a test times out tear_down() will be called after
timed_out() returns.

When a test is in an exclusion group, the tear_down() call is considered part
of the tests run. The next test in the exclusion group will not start until
after tear_down() returns on the current test.

The test's TestHelper is handed to tear_down() and it is permitted to log
messages and call assert functions during tear down.

"""

use "time"

actor PonyTest
  """
  Main test framework actor that organises tests, collates information and
  prints results.
  """

  let _groups: Array[(String, _Group)] = Array[(String, _Group)]
  let _records: Array[_TestRecord] = Array[_TestRecord]
  let _env: Env
  let _timers: Timers = Timers
  var _do_nothing: Bool = false
  var _filter: String = ""
  var _verbose: Bool = false
  var _sequential: Bool = false
  var _no_prog: Bool = false
  var _list_only: Bool = false
  var _started: USize = 0
  var _finished: USize = 0
  var _any_found: Bool = false
  var _all_started: Bool = false

  new create(env: Env, list: TestList tag) =>
    """
    Create a PonyTest object and use it to run the tests from the given
    TestList
    """
    _env = env
    _process_opts()
    _groups.push(("", _SimultaneousGroup))
    list.tests(this)
    _all_tests_applied()

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

    if _list_only then
      // Don't actually run tests, just list them
      _env.out.print(name)
      return
    end

    var index = _records.size()
    _records.push(_TestRecord(_env, name))

    var group = _find_group(test.exclusion_group())
    group(_TestRunner(this, index, consume test, group, _verbose, _env,
      _timers))

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

    for g in _groups.values() do
      if g._1 == name then
        return g._2
      end
    end

    // Group doesn't exist yet, make it.
    // We only need one simultanous group, which we've already made. All new
    // groups are exclusive.
    let g = _ExclusiveGroup
    _groups.push((name, g))
    g

  be _test_started(id: USize) =>
    """
    A test has started running, update status info.
    The id parameter is the test identifier handed out when we created the test
    helper.
    """
    _started = _started + 1

    try
      if not _no_prog then
        _env.out.print(_started.string() + " test" + _plural(_started) +
          " started, " + _finished.string() + " complete: " +
          _records(id).name + " started")
      end
    end

  be _test_complete(id: USize, pass: Bool, log: Array[String] val) =>
    """
    A test has completed, restore its result and update our status info.
    The id parameter is the test identifier handed out when we created the test
    helper.
    """
    _finished = _finished + 1

    try
      _records(id)._result(pass, log)

      if not _no_prog then
        _env.out.print(_started.string() + " test" + _plural(_started) +
          " started, " + _finished.string() + " complete: " +
          _records(id).name + " complete")
      end
    end

    if _all_started and (_finished == _records.size()) then
      // All tests have completed
      _print_report()
    end

  be _all_tests_applied() =>
    """
    All our tests have been handed to apply(), setup for finishing
    """
    if _do_nothing then
      return
    end

    if not _any_found then
      // No tests matched our filter, print special message.
      _env.out.print("No tests found")
      return
    end

    if _list_only then
      // No tests to run
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
    We don't use the options package because we aren't already dependencies.
    """
    var exe_name = ""

    for arg in _env.args.values() do
      if exe_name == "" then
        exe_name = arg
        continue
      end

      if arg == "--sequential" then
        _sequential = true
      elseif arg == "--verbose" then
        _verbose = true
      elseif arg == "--noprog" then
        _no_prog = true
      elseif arg == "--list" then
        _list_only = true
      elseif arg.compare_sub("--filter=", 9) is Equal then
        _filter = arg.substring(9)
      else
        _env.out.print("Unrecognised argument \"" + arg + "\"")
        _env.out.print("")
        _env.out.print("Usage:")
        _env.out.print("  " + exe_name + " [options]")
        _env.out.print("")
        _env.out.print("Options:")
        _env.out.print("  --filter=prefix   - Only run tests whose names " +
          "start with the given prefix.")
        _env.out.print("  --verbose         - Show all test output.")
        _env.out.print("  --sequential      - Run tests sequentially.")
        _env.out.print("  --noprog          - Do not print progress messages.")
        _env.out.print("  --list            - List but do not run tests.")
        _do_nothing = true
        return
      end
    end

  fun _print_report() =>
    """
    The tests are all complete, print out the results.
    """
    var pass_count: USize = 0
    var fail_count: USize = 0

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
    _env.out.print(_Color.green() + "---- Passed: " + pass_count.string() +
      _Color.reset())

    if fail_count == 0 then
      // Success, nothing failed.
      return
    end

    // Not everything passed.
    _env.out.print(_Color.red() + "**** FAILED: " + fail_count.string() +
      " test" + _plural(fail_count) + ", listed below:" + _Color.reset())

    // Finally print our list of failed tests.
    for rec in _records.values() do
      rec._list_failed()
    end

    _env.exitcode(-1)

  fun _plural(n: USize): String =>
    """
    Return a "s" or an empty string depending on whether the given number is 1.
    For use when printing possibly plural words, eg "test" or "tests".
    """
    if n == 1 then "" else "s" end
