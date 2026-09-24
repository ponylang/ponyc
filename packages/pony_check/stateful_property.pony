use "pony_test"

class ref StatefulContext[S, M]
  """
  The system under test and the reference model for the current sample.

  In `step`, the context is `ref` — both `sut` and `model` can be read
  and written. In `invariant` and `final_check`, the context is `box` —
  viewpoint adaptation makes `ctx.sut` and `ctx.model` read-only.

  S and M should be `ref`, `val`, or `tag` capability types. `tag` is
  appropriate when the system under test is an actor. `iso` gives `tag`
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

  S is the system under test (`ref`, `val`, or `tag` for actors). M is
  the reference model (`ref` or `val`). Cmd is a union of `val` command
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

  `h.fail()` flags the property as failed but does not abort the current
  step. Execution continues so that later assertions are still evaluated.
  The `h.assert_*` methods return `Bool` — `false` on failure — which
  can be used for local failure tracking.

  All mutable per-sample state belongs in `ctx.sut` and `ctx.model`,
  not on `self`. The runner reuses the property instance across samples,
  so fields on the implementing class persist between samples and shrink
  replays.

  A property with no `invariant` override and no in-step assertions
  only verifies that `step` does not error.

  Setting `PropertyParams.async` to `true` enables async mode for
  testing actors. In async mode, each step runs as a separate behavior,
  and the next step waits until all expected actions complete. The
  invariant can use `h.expect_action` / `h.complete_action` /
  `h.fail_action` to verify actor state asynchronously, returning
  `true` to indicate that async checks have started.
  """

  fun name(): String
    """
    Appears in test output.
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
    Create a fresh system under test. Called once per sample and once per
    shrink replay. Must be deterministic.
    """

  fun initial_model(): M^
    """
    Create a fresh reference model. Called once per sample and once per
    shrink replay. Must be deterministic.
    """

  fun ref step(
    ctx: StatefulContext[S, M],
    rnd: Randomness,
    h: PropertyHelper)
    : Cmd ?
    """
    Generate and execute one step. Draw a command from `rnd`, apply it
    to `ctx.sut` and `ctx.model`, and return the command object.

    Errors when no valid command exists in the current state. During
    initial recording the runner skips the sample; during shrink replay
    the candidate is rejected.

    Reserve `error` for the case where no valid command exists. For
    operations that should never fail, use `try`/`else` with `h.fail()`.
    For match arms that should be structurally unreachable, use
    `_Unreachable()`.

    Do not call `rnd.start_span` or `rnd.end_span` inside `step`.
    """

  fun invariant(ctx: StatefulContext[S, M] box, h: PropertyHelper): Bool =>
    """
    Check that model and SUT agree after every step. The context is
    `box` (read-only).

    The runner uses the return value for step-level failure attribution.
    Return the assertion result directly:

        fun invariant(ctx: ..., h: PropertyHelper): Bool =>
          h.assert_eq[USize](ctx.model.size(), ctx.sut.size())

    For multiple assertions, chain with `and`:

        fun invariant(ctx: ..., h: PropertyHelper): Bool =>
          h.assert_eq[USize](ctx.model.size(), ctx.sut.size()) and
            h.assert_true(ctx.sut.size() <= ctx.sut.capacity())

    In async mode, use `h.expect_action` / `h.complete_action` /
    `h.fail_action` for asynchronous verification. Return `true` to
    indicate that async checks have started — the next step runs after
    all expected actions complete.

    Default returns `true` (no failure).
    """
    true

  fun final_check(ctx: StatefulContext[S, M] box, h: PropertyHelper): Bool =>
    """
    End-of-sequence check after all steps complete. The context is `box`
    (read-only). Default returns `true` (no failure).
    """
    true

class iso StatefulPropertyUnitTest[S, M, Cmd: Stringable val]
  is UnitTest
  """
  Wraps a StatefulProperty for use as a PonyTest UnitTest.

  Registration:

      test(StatefulPropertyUnitTest[MySut, MyModel, MyCmd](
        _MyStatefulProperty))
  """
  var _prop: (StatefulProperty[S, M, Cmd] iso | None)
  let _name: String

  new iso create(prop: StatefulProperty[S, M, Cmd] iso) =>
    _name = prop.name()
    _prop = consume prop

  fun name(): String => _name

  fun ref apply(h: TestHelper) ? =>
    """
    Run the wrapped stateful property as a PonyTest test.
    """
    let prop = ((_prop = None) as StatefulProperty[S, M, Cmd] iso^)
    let params = prop.params()
    h.long_test(params.timeout)
    let runner =
      StatefulPropertyRunner[S, M, Cmd](
        consume prop, params, h, h, h.env)
    h.dispose_when_done(runner)
    runner.run()
