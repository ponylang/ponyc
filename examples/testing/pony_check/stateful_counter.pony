use "pony_check"
use "pony_test"

class ref _Counter
  var count: USize = 0

  fun ref increment() => count = count + 1

  fun ref decrement() =>
    if count > 0 then count = count - 1 end

primitive _Inc is Stringable
  fun string(): String iso^ => "increment".string()

primitive _Dec is Stringable
  fun string(): String iso^ => "decrement".string()

type _CounterCommand is (_Inc | _Dec)

class iso _CounterProperty
  is StatefulProperty[_Counter, USize, _CounterCommand]
  """
  Verify that a counter tracks its count correctly across
  increment and decrement operations.
  """

  fun name(): String => "stateful/counter"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100)

  fun max_steps(): USize => 20

  fun initial_sut(): _Counter => _Counter

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[_Counter, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : _CounterCommand ?
  =>
    if rnd.bool()? then
      ctx.sut.increment()
      ctx.model = ctx.model + 1
      _Inc
    else
      ctx.sut.decrement()
      if ctx.model > 0 then ctx.model = ctx.model - 1 end
      _Dec
    end

  fun invariant(
    ctx: StatefulContext[_Counter, USize] box,
    h: PropertyHelper)
    : Bool
  =>
    h.assert_eq[USize](ctx.model, ctx.sut.count)
