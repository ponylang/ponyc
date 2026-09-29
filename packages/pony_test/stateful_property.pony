type StepResult[Cmd] is (Cmd | StepReject | StepFail)
  """
  The return type of `StatefulProperty.step()`.
  """

primitive StepReject
  """
  Returned from `step()` when a command is invalid for the current state.
  The runner discards the sample and starts fresh.
  """

primitive StepFail
  """
  Returned from `step()` when the system under test fails. The runner
  records the failing step and activates the shrinker.
  """

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

  `step()` returns `StepResult[Cmd]`. Return the command
  on success, `StepReject` when the command is invalid for the current
  state (the sample is discarded), or `StepFail` when the system under
  test breaks (triggers shrinking).

  Factory methods must be deterministic: given the same starting point,
  they produce equivalent state. They are called once per sample and
  once per shrink replay. Non-deterministic factories cause the shrinker
  to accept or reject candidates incorrectly.

  All mutable per-sample state belongs in `ctx.sut` and `ctx.model`,
  not on `self`. Fields on the implementing class persist between
  samples and shrink replays.
  """

  fun name(): String
    """
    The test name.
    """

  fun params(): PropertyParams =>
    """
    Parameters controlling sample count, seed, shrink budget, and
    timeout.
    """
    PropertyParams

  fun max_steps(): USize =>
    """
    Maximum number of steps per sample. The actual count for each sample
    is drawn uniformly from [1, max_steps()].
    """
    50

  fun initial_sut(): S^
    """
    The initial system under test.
    """

  fun initial_model(): M^
    """
    The initial model state.
    """

  fun ref step(
    ctx: StatefulContext[S, M],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[Cmd]
    """
    Generate and apply a command to `ctx.sut` and `ctx.model`.

    Return the command on success, `StepReject` when the command is
    invalid for the current state, or `StepFail` when the system under
    test breaks. `StepReject` discards the sample and retries.
    `StepFail` records the step and triggers shrinking.
    """

  fun invariant(ctx: StatefulContext[S, M] box, h: PropertyHelper): Bool =>
    """
    Checked after each step. Return `false` to fail the property.

    The context is `box` (read-only). Use `h.assert_*` methods and
    chain with `and`:

        fun invariant(ctx: ..., h: PropertyHelper): Bool =>
          h.assert_eq[USize](ctx.model.size(), ctx.sut.size()) and
            h.assert_true(ctx.sut.size() <= ctx.sut.capacity())

    Default returns `true` (no failure).
    """
    true

  fun final_check(ctx: StatefulContext[S, M] box, h: PropertyHelper): Bool =>
    """
    End-of-sequence check after all steps complete. Not called when
    `step()` returns `StepFail`. The context is `box` (read-only).
    Default returns `true` (no failure).
    """
    true

class iso _StatefulPropertyTest[S, M, Cmd: Stringable val]
  is UnitTest
  """
  Wraps a StatefulProperty for use as a PonyTest UnitTest.
  """
  var _prop: (StatefulProperty[S, M, Cmd] iso | None)
  let _name: String

  new iso create(
    prop: StatefulProperty[S, M, Cmd] iso,
    name': (String | None) = None)
  =>
    _name =
      match \exhaustive\ name'
      | None => prop.name()
      | let s: String => s
      end
    _prop = consume prop

  fun name(): String => _name

  fun ref apply(h: TestHelper) ? =>
    """
    Run the wrapped stateful property test.
    """
    let prop = ((_prop = None) as StatefulProperty[S, M, Cmd] iso^)
    let params = prop.params()
    let exec =
      recover iso
        _StatefulPropertyExec[S, M, Cmd](consume prop, params, h, h.env)
      end
    h._start_property(consume exec)
