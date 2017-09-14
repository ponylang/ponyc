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
