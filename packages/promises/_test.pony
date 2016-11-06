use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestPromise)

class iso _TestPromise is UnitTest
  fun name(): String => "promises/Promise"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _test_fulfilled(h)
    _test_rejected(h)

  fun _test_fulfilled(h: TestHelper) =>
    h.expect_action("fulfilled")
    let p = Promise[String]
    p.next[None]({(s: String)(h) => h.complete_action(s) } iso)
    p("fulfilled")

  fun _test_rejected(h: TestHelper) =>
    h.expect_action("rejected")
    let p = Promise[String]
    p.next[String](
      {(s: String)(h): String ? => error } iso,
      {()(h): String => "rejected" } iso
    ).next[None](
      {(s: String)(h) => h.complete_action(s) } iso
    )
    p.reject()
