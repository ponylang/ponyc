use "collections"

actor Timers
  """
  A hierarchical set of timing wheels.
  """
  var _current: U64 = 0
  let _slop: U64
  let _map: MapIs[Timer tag, Timer] = MapIs[Timer tag, Timer]
  let _wheel: Array[_TimingWheel] = Array[_TimingWheel](_wheels())
  let _pending: List[Timer] = List[Timer]
  var _event: EventID = Event.none()

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
      _wheel.push(_TimingWheel(i))
    end

  be apply(timer: Timer iso) =>
    """
    Sets a timer. Fire it if need be, schedule it on the right timing wheel,
    then rearm the timer.
    """
    let timer': Timer ref = consume timer
    _map(timer') = timer'
    timer'._slop(_slop)
    _fire(timer')
    _advance()

  be cancel(timer: Timer tag) =>
    """
    Cancels a timer.
    """
    try
      (_, let timer') = _map.remove(timer)
      timer'._cancel()
    end

  be dispose() =>
    """
    Dipose of this set of timing wheels.
    """
    for wheel in _wheel.values() do
      wheel.clear()
    end
    _map.clear()

    if not _event.is_null() then
      @asio_event_unsubscribe[None](_event)
      _event = Event.none()
    end

  be _event_notify(event: EventID, flags: U32, arg: U64) =>
    """
    When the event fires, advance the timing wheels.
    """
    if Event.disposable(flags) then
      @asio_event_destroy[None](event)
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

    var nsec = _next()

    if _event.is_null() then
      if nsec != -1 then
        // Create a new event.
        _event = @asio_event_create[Pointer[Event]](this, nsec, U32(4), true)
      end
    else
      if nsec != -1 then
        // Update an existing event.
        @asio_event_update[None](_event, nsec)
      else
        // Unsubscribe an existing event.
        @asio_event_unsubscribe[None](_event)
        _event = Event.none()
      end
    end

  fun ref _fire(timer: Timer) =>
    """
    Fire a timer if necessary, then schedule it on the correct timing wheel
    based on how long it is until it expires.
    """
    if not timer._fire(_current) then
      try
        _map.remove(timer)
      end
      return
    end

    try
      let rem = timer._next() - _current
      _get_wheel(rem).schedule(consume timer)
    end

  fun _next(): U64 =>
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
