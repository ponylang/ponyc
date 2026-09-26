class ref StatefulContext[S, M]
  """
  The system under test and the reference model for the current sample.

  In `step`, the context is `ref` — both `sut` and `model` can be read
  and written. In `invariant` and `final_check`, the context is `box` —
  viewpoint adaptation makes `ctx.sut` and `ctx.model` read-only.

  S and M should be `ref` or `val` capability types. `iso` gives `tag`
  through the `box` view (unusable); `trn` gives `box` where mutation
  was expected.
  """
  var sut: S
  var model: M

  new ref create(sut': S^, model': M^) =>
    sut = consume sut'
    model = consume model'

trait StatefulProperty[S, M, Cmd: Stringable val]
  """
  A stateful property test with interleaved command generation and
  execution.

  S is the system under test (`ref` or `val` capability). M is the
  reference model (`ref` or `val`). Cmd is a union of `val` command
  classes, each `Stringable`.

  Each sample creates fresh state via `initial_sut()` and
  `initial_model()`, draws a step count uniformly from
  [1, max_steps()], then for each step calls `step` to generate and
  execute a command, followed by `invariant` to check model-SUT
  agreement. After all steps, `final_check` runs once. On failure, the
  choice sequence is replayed with shrunken candidates.

  Factory methods must be deterministic: given the same starting point,
  they produce equivalent state. They are called once per sample and
  once per shrink replay. Non-deterministic factories cause the shrinker
  to accept or reject candidates incorrectly.

  All mutable per-sample state belongs in `ctx.sut` and `ctx.model`,
  not on `self`. The runner reuses the property instance across samples,
  so fields on the implementing class persist between samples and shrink
  replays.
  """

  fun name(): String

  fun params(): PropertyParams =>
    PropertyParams

  fun max_steps(): USize =>
    50

  fun initial_sut(): S^

  fun initial_model(): M^

  fun ref step(
    ctx: StatefulContext[S, M],
    rnd: Randomness,
    h: TestHelper)
    : Cmd ?

  fun invariant(ctx: StatefulContext[S, M] box, h: TestHelper): Bool =>
    true

  fun final_check(ctx: StatefulContext[S, M] box, h: TestHelper): Bool =>
    true

class iso StatefulPropertyTest[S, M, Cmd: Stringable val]
  is UnitTest
  """
  Wraps a StatefulProperty for use as a PonyTest UnitTest.
  """
  var _prop: (StatefulProperty[S, M, Cmd] iso | None)
  let _name: String

  new iso create(prop: StatefulProperty[S, M, Cmd] iso) =>
    _name = prop.name()
    _prop = consume prop

  fun name(): String => _name

  fun ref apply(h: TestHelper) ? =>
    let prop = ((_prop = None) as StatefulProperty[S, M, Cmd] iso^)
    let params = prop.params()
    h.long_test(params.timeout)
    let exec =
      recover iso
        _StatefulPropertyExec[S, M, Cmd](consume prop, params, h, h.env)
      end
    h._start_property(consume exec)
