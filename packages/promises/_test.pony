use "pony_test"
use "time"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestFlattenNextFirstHasFulfillError)
    test(_TestFlattenNextHappyPath)
    test(_TestFlattenNextRejectFirst)
    test(_TestFlattenNextSecondHasFulfillError)
    test(_TestPromise)
    test(_TestPromiseAdd)
    test(_TestPromiseSelect)
    test(_TestPromisesJoin)
    test(_TestPromisesJoinThenReject)
    test(_TestPromiseTimeout)

class \nodoc\ iso _TestPromise is UnitTest
  fun name(): String => "promises/Promise"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _test_fulfilled(h)
    _test_rejected(h)

  fun _test_fulfilled(h: TestHelper) =>
    h.expect_action("fulfilled")
    let p = Promise[String]
    p.next[None]({(s) => h.complete_action(s) })
    p("fulfilled")

  fun _test_rejected(h: TestHelper) =>
    h.expect_action("rejected")
    let p = Promise[String]
    p
      .next[String](
        {(_): String ? => error },
        {(): String => "rejected" })
      .next[None](
        {(s) => h.complete_action(s) })

    p.reject()

class \nodoc\ iso _TestPromiseAdd is UnitTest
  fun name(): String => "promises/Promise.add"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)

    (var p1, var p2) = (Promise[I64], Promise[I64])
    h.expect_action("5")

    (p1 + p2)
      .next[None]({(ns) => h.complete_action((ns._1 + ns._2).string()) })

    p1(2)
    p2(3)

    (p1, p2) = (Promise[I64], Promise[I64])
    h.expect_action("reject p1")

    (p1 + p2)
      .next[None](
        {(_) => h.complete(false) },
        {() => h.complete_action("reject p1") })

    p1.reject()
    p2(3)

    (p1, p2) = (Promise[I64], Promise[I64])
    h.expect_action("reject p2")

    (p1 + p2)
      .next[None](
        {(_) => h.complete(false) },
        {() => h.complete_action("reject p2") })

    p1(2)
    p2.reject()

class \nodoc\ iso _TestPromiseSelect is UnitTest
  fun name(): String => "promises/Promise.select"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    h.expect_action("a")
    h.expect_action("b")

    let pa = Promise[String]
    let pb = Promise[String] .> next[None]({(s) => h.complete_action(s) })

    pa
      .select(pb)
      .next[None](
        {(r) =>
          h.complete_action(r._1)
          r._2("b")
        })

    pa("a")

class \nodoc\ iso _TestPromiseTimeout is UnitTest
  fun name(): String => "promises/Promise.timeout"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    h.expect_action("timeout")
    Promise[None]
      .> timeout(1000)
      .next[None](
        FulfillIdentity[None],
        {() => h.complete_action("timeout") } iso)

class \nodoc\ iso _TestPromisesJoin is UnitTest
  fun name(): String => "promises/Promise.join"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    h.expect_action("abc")
    (let a, let b, let c) = (Promise[String], Promise[String], Promise[String])
    let abc = Promises[String].join([a; b; c].values())
      .next[String]({(l) => String.join(l.values()) })
      .next[None]({(s) =>
        if
          (s.contains("a") and s.contains("b")) and
          (s.contains("c") and (s.size() == 3))
        then
          h.complete_action("abc")
        end
      })

    a("a")
    b("b")
    c("c")

class \nodoc\ iso _TestPromisesJoinThenReject is UnitTest
  fun name(): String => "promises/Promise.join"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    h.expect_action("rejected")
    (let a, let b, let c) = (Promise[String], Promise[String], Promise[String])
    let abc = Promises[String].join([a; b; c].values())
      .next[String]({(l) => String.join(l.values()) },
        {() => h.complete_action("rejected"); "string"})

    a("a")
    b("b")
    c.reject()

class \nodoc\ iso _TestFlattenNextHappyPath is UnitTest
  """
  This is the happy path, everything works `flatten_next` test.

  If a reject handler, gets called, it would be a bug.
  """
  fun name(): String =>
    "promises/Promise.flatten_next_happy_path"

  fun apply(h: TestHelper) =>
    let initial_string = "foo"
    let second_string = "bar"

    h.long_test(2_000_000_000)
    h.expect_action(initial_string)
    h.expect_action(second_string)

    let start = Promise[String]
    let inter = start.flatten_next[String](
      _FlattenNextFirstPromise~successful_fulfill(
        second_string, h, initial_string),
      _FlattenNextFirstPromise~fail_if_reject_is_called(h)
    )
    inter.next[None](
      _FlattenNextSecondPromise~successful_fulfill(h, second_string),
      _FlattenNextSecondPromise~fail_if_reject_is_called(h)
    )

    start(initial_string)

class \nodoc\ iso _TestFlattenNextFirstHasFulfillError is UnitTest
  """
  Two promises chained `flatten_next` test where the fulfill action for the
  first promise fails.

  This should result in the rejection handler for the second promise being run.

  Neither the rejection handler for the first promise, nor the fulfillment
  handler for the second promise should be run.
  """
  fun name(): String =>
    "promises/Promise.flatten_next_first_has_fulfill_error"

  fun apply(h: TestHelper) =>
    let initial_string = "foo"
    let expected_reject_string = "rejected"

    h.long_test(2_000_000_000)
    h.expect_action(initial_string)
    h.expect_action(expected_reject_string)

    let start = Promise[String]
    let inter = start.flatten_next[String](
      _FlattenNextFirstPromise~fulfill_will_error(h, initial_string),
      _FlattenNextFirstPromise~fail_if_reject_is_called(h)
    )
    inter.next[None](
      _FlattenNextSecondPromise~fail_if_fulfill_is_called(h),
      _FlattenNextSecondPromise~reject_expected(h, expected_reject_string)
    )

    start(initial_string)


class \nodoc\ iso _TestFlattenNextSecondHasFulfillError is UnitTest
  """
  Two promises chained `flatten_next` test where the fulfill action for the
  second promise fails.

  Neither the reject handler should be called.
  """
  fun name(): String =>
    "promises/Promise.flatten_next_second_has_fulfill_error"

  fun apply(h: TestHelper) =>
    let initial_string = "foo"
    let second_string = "bar"

    h.long_test(2_000_000_000)
    h.expect_action(initial_string)
    h.expect_action(second_string)

    let start = Promise[String]
    let inter = start.flatten_next[String](
      _FlattenNextFirstPromise~successful_fulfill(
        second_string, h, initial_string),
      _FlattenNextFirstPromise~fail_if_reject_is_called(h)
    )
    inter.next[None](
      _FlattenNextSecondPromise~fulfill_will_error(h, second_string),
      _FlattenNextSecondPromise~fail_if_reject_is_called(h)
    )

    start(initial_string)

class \nodoc\ iso _TestFlattenNextRejectFirst is UnitTest
  """
  Two promises chained `flatten_next` test where the first promise is rejected.

  The reject action for both promises should be run.

  Neither fulfill handler should be run.
  """
  fun name(): String =>
    "promises/Promise.flatten_next_reject_first"

  fun apply(h: TestHelper) =>
    let initial_string = "foo"
    let first_reject = "first rejected"
    let second_reject = "second rejected"

    h.long_test(2_000_000_000)
    h.expect_action(first_reject)
    h.expect_action(second_reject)

    let start = Promise[String]
    let inter = start.flatten_next[String](
      _FlattenNextFirstPromise~fail_if_fulfill_is_called(h),
      _FlattenNextFirstPromise~reject_expected(h, first_reject)
    )
    inter.next[None](
      _FlattenNextSecondPromise~fail_if_fulfill_is_called(h),
      _FlattenNextSecondPromise~reject_expected(h, second_reject)
    )

    start.reject()

primitive \nodoc\ _FlattenNextFirstPromise
  """
  Callback functions called after fulfilled/reject is called on the 1st
  promise in our `flatten_next` tests.

  All of the `flatten_next` tests a 2nd promise chained so none of these
  actions should ever call `complete` on the `TestHelper` as there's another
  promise in the chain.
  """
  fun successful_fulfill(send_on: String, h: TestHelper,
    expected: String,
    actual: String): Promise[String]
  =>
    h.assert_eq[String](expected, actual)
    h.complete_action(expected)
    let p = Promise[String]
    p(send_on)
    p

  fun fulfill_will_error(h: TestHelper,
    expected: String,
    actual: String): Promise[String] ?
  =>
    h.assert_eq[String](expected, actual)
    h.complete_action(expected)
    error

  fun fail_if_fulfill_is_called(h: TestHelper, s: String): Promise[String] =>
    h.fail("fulfill shouldn't have been run on _FlattenNextFirstPromise")
    Promise[String]

  fun reject_expected(h: TestHelper, action: String): Promise[String] ? =>
    h.complete_action(action)
    error

  fun fail_if_reject_is_called(h: TestHelper): Promise[String] =>
    h.fail("reject shouldn't have been run on _FlattenNextFirstPromise")
    Promise[String]

primitive \nodoc\ _FlattenNextSecondPromise
  """
  Callback functions called after fulfilled/reject is called on the second
  promise in our `flatten_next` tests.

  This helper is designed for tests where there are only two promises. there
  are calls to `complete` on `TestHelper` that would be incorrectly placed if
  additional promises were chained after this. If tests are added with longer
  chains, this helper's usage of `complete` should be reworked accordingly.
  """
  fun successful_fulfill(h: TestHelper, expected: String, actual: String) =>
    h.assert_eq[String](expected, actual)
    h.expect_action(expected)
    h.complete(true)

  fun fulfill_will_error(h: TestHelper, expected: String, actual: String) ? =>
    h.assert_eq[String](expected, actual)
    h.complete_action(expected)
    h.complete(true)
    error

  fun fail_if_fulfill_is_called(h: TestHelper, s: String) =>
    h.fail("fulfill shouldn't have been run on _FlattenNextSecondPromise")

  fun reject_expected(h: TestHelper, action: String) =>
    h.complete_action(action)
    h.complete(true)

  fun fail_if_reject_is_called(h: TestHelper) =>
    h.fail("reject shouldn't have been run on _FlattenNextSecondPromise")

