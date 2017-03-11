use "ponytest"

class iso _TestValtrace is UnitTest
  """
  Test val trace optimisation
  """
  fun name(): String => "builtin/Valtrace"

  fun apply(h: TestHelper) =>
    _Valtrace.one(h)
    h.long_test(2_000_000_000) // 2 second timeout

actor _Valtrace
  var count: U32 = 0

  be one(h: TestHelper) =>
    """
    Create a String iso, send it to a new actor.
    """
    @pony_triggergc[None](this)
    let s = recover String.>append("test") end
    _Valtrace.two(this, h, consume s)

  be two(a1: _Valtrace, h: TestHelper, s: String iso) =>
    """
    Receive a String iso allocated by a different actor.
    Append to it.
    Send it as a val to a third actor.
    """
    @pony_triggergc[None](this)
    s.append("ing")
    _Valtrace.three(a1, this, h, consume s)

  be three(a1: _Valtrace, a2: _Valtrace, h: TestHelper, s: String) =>
    """
    Receive a String that was an iso that passed through another actor.
    """
    @pony_triggergc[None](this)
    h.assert_eq[String]("testing", s)
    _Valtrace.four(a1, a2, this, h, s)

  be four(a1: _Valtrace, a2: _Valtrace, a3: _Valtrace,
    h: TestHelper, s: String)
  =>
    """
    Ask all actors to test the string.
    """
    @pony_triggergc[None](this)
    a1.gc(a1, h, s)
    a2.gc(a1, h, s)
    a2.gc(a1, h, s)
    gc(a1, h, s)

  be gc(a: _Valtrace, h: TestHelper, s: String) =>
    @pony_triggergc[None](this)
    h.assert_eq[String]("testing", s)
    a.done(h)

  be done(h: TestHelper) =>
    count = count + 1

    if count == 4 then
      h.complete(true)
    end
