use "pony_test"

// As long as #4284 is fixed, this program will run to completion and
// _TestIssue4284 will pass. If a regression is introduced, this
// program will crash.

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  fun tag tests(test: PonyTest) =>
    test(_TestIssue4284)

class iso _TestIssue4284 is UnitTest
  fun name(): String => "_TestIssue4284"

  fun apply(h: TestHelper) =>
    let mutable = MutableState

    var i: USize = 0
    while i < 10 do i = i + 1
      SomeActor.trace(HasOpaqueReferenceToMutableState(mutable), 100_000)
    end

    h.long_test(1_000_000_000_000)
    h.complete(true)

class MutableState
  let _inner: ImmutableStateInner = ImmutableStateInner
  new create() => None

class val ImmutableStateInner
  let _data: Array[String] = []
  new val create() => None

class val HasOpaqueReferenceToMutableState
  let _opaque: MutableState tag
  new val create(opaque: MutableState tag) => _opaque = opaque

actor SomeActor
  be trace(immutable: HasOpaqueReferenceToMutableState, count: USize) =>
    if count > 1 then
      trace(immutable, count - 1)
    end
