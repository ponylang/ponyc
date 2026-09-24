use "pony_check"
use "pony_test"

actor _ActorCounter
  var _count: USize = 0

  be increment() => _count = _count + 1

  be decrement() =>
    if _count > 0 then _count = _count - 1 end

  be get_count(cb: {(USize)} val) => cb(_count)

primitive _ActorInc is Stringable
  fun string(): String iso^ => "increment".string()

primitive _ActorDec is Stringable
  fun string(): String iso^ => "decrement".string()

type _ActorCounterCommand is (_ActorInc | _ActorDec)

class iso _AsyncCounterProperty
  is StatefulProperty[_ActorCounter tag, USize, _ActorCounterCommand]
  """
  Verify an actor-based counter using async stateful property testing.

  The system under test is an actor (`tag` capability). Each step sends
  a message, and the invariant queries the actor asynchronously —
  comparing internal state against the model via a callback.
  """

  fun name(): String => "stateful/async_counter"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50, async' = true)

  fun max_steps(): USize => 15

  fun initial_sut(): _ActorCounter tag => _ActorCounter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_ActorCounter tag, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _ActorCounterCommand ?
  =>
    if rnd.bool()? then
      ctx.model = ctx.model + 1
      ctx.sut.increment()
      _ActorInc
    else
      if ctx.model > 0 then ctx.model = ctx.model - 1 end
      ctx.sut.decrement()
      _ActorDec
    end

  fun invariant(
    ctx: StatefulContext[_ActorCounter tag, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    let expected = ctx.model
    h.expect_action("check_count")
    ctx.sut.get_count({(actual: USize)(expected, h) =>
      if expected != actual then
        h.fail_action("check_count")
      else
        h.complete_action("check_count")
      end
    } val)
    true
