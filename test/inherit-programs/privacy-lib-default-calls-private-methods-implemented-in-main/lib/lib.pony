trait CounterIncrementer
  fun _current_counter_value(): U64
  fun ref _set_counter_value(v: U64)
  fun ref increment_counter() =>
    let v = _current_counter_value() + 1
    _set_counter_value(v)
