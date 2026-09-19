## Add stateful property testing to PonyCheck

PonyCheck now supports stateful property testing, where each sample creates a system under test and a reference model, then runs a sequence of randomly generated steps that operate on both. After each step, an invariant checks that the model and the SUT agree. On failure, the choice sequence is shrunk to find a minimal reproducing case.

```pony
use "pony_check"

class iso _CounterProperty
  is StatefulProperty[_Counter, USize, _Increment]
  fun name(): String => "counter"
  fun max_steps(): USize => 20
  fun initial_sut(): _Counter => _Counter
  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_Counter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _Increment
  =>
    ctx.sut.increment()
    ctx.model = ctx.model + 1
    _Increment

  fun invariant(
    ctx: StatefulContext[_Counter, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    h.assert_eq[USize](ctx.model, ctx.sut.count)
```

Register with PonyTest using the `StatefulPropertyUnitTest` adapter:

```pony
test(StatefulPropertyUnitTest[_Counter, USize, _Increment](
  _CounterProperty))
```

The `step` method draws randomness, applies a command to both the SUT and the model, and returns a `Stringable val` command object used in failure reporting. When no valid command exists in the current state, `step` errors and the runner retries with a fresh sample.
