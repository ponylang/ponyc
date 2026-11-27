// T's default _increment_counter calls the private methods
// _current_counter_value and _set_counter_value, which Counter implements in
// the same package. Counter inherits the default and must print 2.
trait CounterIncrementer
  fun _current_counter_value(): U64
  fun ref _set_counter_value(v: U64)
  fun ref _increment_counter() =>
    let v = _current_counter_value() + 1
    _set_counter_value(v)

class Counter is CounterIncrementer
  var _counter: U64 = 1
  fun _current_counter_value(): U64 => _counter
  fun ref _set_counter_value(v: U64) => _counter = v
  fun ref bump(): U64 => _increment_counter(); _counter

actor Main
  new create(env: Env) => env.out.print(Counter.bump().string())
