use "collections"
use "events"

actor Timers
  """
  A hierarchical set of timing wheels.
  """
  var _current: U64 = 0
  let _slop: U64
  let _map: Map[Identity[Timer tag], Timer] = Map[Identity[Timer tag], Timer]
  let _wheel: Array[_TimingWheel] = Array[_TimingWheel](_wheels())
  let _pending: List[Timer] = List[Timer]
  var _event: Pointer[Event] tag = Pointer[Event]

  new create(slop: U64 = 20) =>
    """
    Create a timer handler with the specified number of slop bits. No slop bits
    means trying for nanosecond resolution. 10 slop bits is approximately
    microsecond resolution, 20 slop bits is approximately millisecond
    resolution.
    """
    _slop = slop
    _set_time()

    for i in Range(0, _wheels()) do
      _wheel.append(_TimingWheel(i))
    end

  be apply(timer: Timer iso) =>
    """
    Sets a timer. Fire it if need be, schedule it on the right timing wheel,
    then rearm the timer.
    """
    let timer': Timer ref = consume timer
    _map(Identity[Timer tag](timer')) = timer'
    timer'._slop(_slop)
    _fire(timer')
    _advance()

  be cancel(timer: Timer tag) =>
    """
    Cancels a timer.
    """
    try
      let timer' = _map.remove(Identity[Timer tag](timer))
      timer'._cancel()
    end

  be dispose() =>
    """
    Dipose of this set of timing wheels.
    """
    try
      for wheel in _wheel.values() do
        wheel.clear()
      end
      _map.clear()
    end

    _event = Event.timer(this, _event, -1)

  be _event_notify(event: Pointer[Event] tag, flags: U32) =>
    """
    When the event fires, advance the timing wheels.
    """
    if Event.disposable(flags) then
      Event.dispose(event)
    elseif event is _event then
      _advance()
    end

  fun ref _advance() =>
    """
    Update the current time, process all the timing wheels, and set the event
    for the next time we need to advance.
    """
    let elapsed = _set_time()

    try
      for i in Range(0, _wheels()) do
        if not _wheel(i).advance(_pending, _current, elapsed) then
          break
        end
      end

      for timer in _pending.values() do
        _fire(timer)
      end
    end

    _pending.clear()
    _event = Event.timer(this, _event, _next())

  fun ref _fire(timer: Timer) =>
    """
    Fire a timer if necessary, then schedule it on the correct timing wheel
    based on how long it is until it expires.
    """
    if not timer._fire(_current) then
      try
        _map.remove(Identity[Timer tag](timer))
      end
      return
    end

    try
      let rem = timer._next() - _current
      _get_wheel(rem).schedule(consume timer)
    end

  fun box _next(): U64 =>
    """
    Return the next time at which the timing wheels should be advanced. This is
    adjusted for slop, so it yields nanoseconds. If no events are pending, this
    returns -1.
    """
    var next: U64 = -1

    try
      for i in Range(0, _wheels()) do
        next = next.min(_wheel(i).next(_current))
      end
    end

    if next != -1 then
      next = next << _slop
    end

    next

  fun ref _set_time(): U64 =>
    """
    Set the current time with precision reduced by the slop bits. Return the
    elapsed time.
    """
    let previous = _current = Time.nanos() >> _slop
    _current - previous

  fun ref _get_wheel(rem: U64): _TimingWheel ? =>
    """
    Get the hierarchical timing wheel for the given time until expiration.
    """
    let t = rem.min(_expiration_max())
    let i = ((t.bitwidth() - t.clz()) - 1) / _bits()
    _wheel(i)

  fun tag _expiration_max(): U64 =>
    """
    Get the maximum time the timing wheels cover. Anything beyond this is
    scheduled on the last timing wheel.
    """
    (1 << (_wheels() * _bits())) - 1

  fun tag _wheels(): U64 => 4
  fun tag _bits(): U64 => 6
