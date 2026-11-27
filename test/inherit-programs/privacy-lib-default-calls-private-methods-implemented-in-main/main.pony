// lib's default increment_counter calls the private methods
// _current_counter_value and _set_counter_value, which main's Counter
// implements. Counter inherits the default and must print 2.
use l = "lib"
class Counter is l.CounterIncrementer
  var _counter: U64 = 1
  fun _current_counter_value(): U64 => _counter
  fun ref _set_counter_value(v: U64) => _counter = v
  fun ref bump(): U64 => increment_counter(); _counter

actor Main
  new create(env: Env) => env.out.print(Counter.bump().string())
