use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestCustodian)
    test(_TestRegistrar)

class iso _TestCustodian is UnitTest
  """
  Dispose of an actor using a Custodian.
  """
  fun name(): String => "bureaucracy/Custodian"

  fun ref apply(h: TestHelper) =>
    h.long_test(2_000_000_000) // 2 second timeout
    let c = Custodian
    c(_TestDisposable(h))
    c.dispose()

class iso _TestRegistrar is UnitTest
  """
  Register an actor and retrieve it.
  """
  fun name(): String => "bureaucracy/Registrar"

  fun ref apply(h: TestHelper) =>
    h.long_test(2_000_000_000) // 2 second timeout
    let r = Registrar
    r("test") = _TestDisposable(h)

    r[_TestDisposable]("test").next[None](
      recover
        lambda(value: _TestDisposable)(h) =>
          value.dispose()
        end
      end)

actor _TestDisposable
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  be dispose() =>
    _h.complete(true)
