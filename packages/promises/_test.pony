use "ponytest"
use "time"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestPromise)
    test(_TestPromiseAdd)
    test(_TestPromiseSelect)
    test(_TestPromiseTimeout)
    test(_TestPromisesJoin)
    test(_TestPromisesJoinThenReject)
    test(_TestFlattenNextHappyPath)
    test(_TestFlattenNextStartFulfillErrors)
    test(_TestFlattenNextInterFulfillErrors)
    test(_TestFlattenNextStartRejected)

class iso _TestPromise is UnitTest
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

class iso _TestPromiseAdd is UnitTest
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

class iso _TestPromiseSelect is UnitTest
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

class iso _TestPromiseTimeout is UnitTest
  fun name(): String => "promises/Promise.timeout"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    h.expect_action("timeout")
    Promise[None]
      .> timeout(1000)
      .next[None](
        FulfillIdentity[None],
        {() => h.complete_action("timeout") } iso)

class iso _TestPromisesJoin is UnitTest
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

class iso _TestPromisesJoinThenReject is UnitTest
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

class iso _TestFlattenNextHappyPath is UnitTest
  fun name(): String => "promises/Promise.flatten_next/happy_path"

  fun apply(h: TestHelper) =>
    let initial_string = "foo"
    let second_string = "bar"

    h.long_test(2_000_000_000)
    h.expect_action(initial_string)
    h.expect_action(second_string)

    let start = Promise[String]
    let inter = start.flatten_next[String](
      _FlattenNextStartHelper~successful_fulfill(
        second_string, h, initial_string),
      _FlattenNextStartHelper~reject_fail_if_called(h)
    )
    inter.next[None](
      _FlattenNextInterHelper~assert_equality(h, second_string),
      _FlattenNextInterHelper~reject_fail_if_called(h)
    )

    start(initial_string)

class iso _TestFlattenNextStartFulfillErrors is UnitTest
  fun name(): String => "promises/Promise.flatten_next/start_fulfill_errors"
  fun apply(h: TestHelper) =>
    let initial_string = "foo"
    let expected_reject_string = "rejected"

    h.long_test(2_000_000_000)
    h.expect_action(initial_string)
    h.expect_action(expected_reject_string)

    let start = Promise[String]
    let inter = start.flatten_next[String](
      _FlattenNextStartHelper~error_fulfill(h, initial_string)
    )
    inter.next[None](
      _FlattenNextInterHelper~fulfill_fail_if_called(h),
      _FlattenNextInterHelper~reject_expected(h, expected_reject_string)
    )

    start(initial_string)


class iso _TestFlattenNextInterFulfillErrors is UnitTest
  fun name(): String => "promises/Promise.flatten_next/inter_fulfill_errors"
  fun apply(h: TestHelper) =>
    let initial_string = "foo"
    let second_string = "bar"
    let expected_reject_string = "rejected"

    h.long_test(2_000_000_000)
    h.expect_action(initial_string)
    h.expect_action(second_string)
    h.expect_action(expected_reject_string)

    let start = Promise[String]
    let inter = start.flatten_next[String](
      _FlattenNextStartHelper~successful_fulfill(
        second_string, h, initial_string),
      _FlattenNextStartHelper~reject_fail_if_called(h)
    )
    inter.next[None](
      _FlattenNextInterHelper~error_fulfill(h, second_string),
      _FlattenNextInterHelper~reject_expected(h, expected_reject_string)
    )

    start(initial_string)

class iso _TestFlattenNextStartRejected is UnitTest
  fun name(): String => "promises/Promise.flatten_next/start_rejected"

  fun apply(h: TestHelper) =>
    let initial_string = "foo"
    let first_reject = "first rejected"
    let second_reject = "second rejected"

    h.long_test(2_000_000_000)
    h.expect_action(first_reject)
    h.expect_action(second_reject)

    let start = Promise[String]
    let inter = start.flatten_next[String](
      _FlattenNextStartHelper~fulfill_fail_if_called(h),
      _FlattenNextStartHelper~reject_expected(h, first_reject)
    )
    inter.next[None](
      _FlattenNextInterHelper~fulfill_fail_if_called(h),
      _FlattenNextInterHelper~reject_expected(h, second_reject)
    )

    start.reject()

primitive _FlattenNextStartHelper
  fun successful_fulfill(send_on: String, h: TestHelper,
    expected: String,
    actual: String): Promise[String]
  =>
    h.assert_eq[String](expected, actual)
    h.complete_action(expected)
    let p = Promise[String]
    p(send_on)
    p

  fun reject_fail_if_called(h: TestHelper): Promise[String] =>
    h.fail("_FlattenNextStartHelper: reject_fail_if_called")
    Promise[String]

  fun fulfill_fail_if_called(h: TestHelper, s: String): Promise[String] =>
    h.fail("_FlattenNextStartHelper: fulfill_fail_if_called")
    Promise[String]

  fun error_fulfill(h: TestHelper,
    expected: String,
    actual: String): Promise[String] ?
  =>
    h.assert_eq[String](expected, actual)
    error

  fun reject_expected(h: TestHelper, action: String): Promise[String] ? =>
    h.complete_action(action)
    error

primitive _FlattenNextInterHelper
  fun assert_equality(h: TestHelper, expected: String, actual: String) =>
    h.assert_eq[String](expected, actual)
    h.expect_action(expected)
    h.complete(true)

  fun reject_fail_if_called(h: TestHelper) =>
    h.fail("_FlattenNextInterHelper- reject_fail_if_called")

  fun fulfill_fail_if_called(h: TestHelper, s: String) =>
    h.fail("_FlattenNextInterHelper- fulfill_fail_if_called")

  fun reject_expected(h: TestHelper, action: String) =>
    h.complete_action(action)
    h.complete(true)

  fun error_fulfill(h: TestHelper, expected: String, actual: String) ?
  =>
    h.assert_eq[String](expected, actual)
    h.complete_action(expected)
    error
