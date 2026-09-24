## Add async stateful property testing to PonyCheck

Stateful properties can now test actors. Setting `async' = true` in `PropertyParams` runs the step loop as a behavior chain: each step executes, its invariant queries the system under test asynchronously through the action mechanism (`expect_action` / `complete_action` / `fail_action`), and the next step starts only after all expected actions complete.

```pony
use "pony_check"

primitive _Increment is Stringable
  fun string(): String iso^ => "increment".string()

type _CounterCommand is _Increment

actor _ActorCounter
  var _count: USize = 0
  be increment() => _count = _count + 1
  be get_count(cb: {(USize)} val) => cb(_count)

class iso _AsyncCounterProperty
  is StatefulProperty[_ActorCounter tag, USize, _CounterCommand]

  fun name(): String => "async_counter"
  fun params(): PropertyParams =>
    PropertyParams(where async' = true)
  fun max_steps(): USize => 10
  fun initial_sut(): _ActorCounter tag => _ActorCounter
  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_ActorCounter tag, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _CounterCommand ?
  =>
    ctx.sut.increment()
    ctx.model = ctx.model + 1
    _Increment

  fun invariant(
    ctx: StatefulContext[_ActorCounter tag, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    let expected = ctx.model
    h.expect_action("check")
    ctx.sut.get_count({(actual: USize)(expected, h) =>
      if expected == actual then
        h.complete_action("check")
      else
        h.fail_action("check")
      end
    } val)
    true
```

The SUT type parameter is `_ActorCounter tag` — a `tag` reference to an actor. The invariant registers an expected action, queries the actor, and completes or fails the action in the callback. The invariant returns `true` to indicate that the async check has started, not that it passed.

Shrinking and regression replay use the same async step mechanism.
