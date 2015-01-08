use "collections"

class _TimingWheel
  """
  A timing wheel in a hierarchical set of timing wheels. Each wheel covers 6
  bits of precision.
  """
  let _index: U64
  let _shift: U64
  let _adjust: U64
  var _pending: U64 = 0
  let _list: Array[List[Timer]]

  new create(index: U64) =>
    """
    Create a timing wheel at the given hierarchical level.
    """
    _index = index
    _shift = index * _bits()
    _adjust = if index > 0 then 1 else 0 end
    _list = Array[List[Timer]](_max())

    for i in Range(0, _max()) do
      _list.append(List[Timer])
    end

  fun ref schedule(timer: Timer) =>
    """
    Schedule a timer on this wheel. Mark the bit indicating that the given slot
    has timers in its list.
    """
    let slot = ((timer._next() >> _shift) - _adjust) and _mask()

    try
      let list = _list(slot)
      _list(slot).append(timer._get_node())
      _pending = _pending or (1 << slot)
    end

  fun ref advance(list: List[Timer], current: U64, elapsed: U64): Bool =>
    """
    Remove pending timers from this timing wheel and put them on the pending
    list supplied. Needs the current time and the elapsed time since the
    previous advance. Returns true if the next timing wheel in the hierarchy
    should be advanced.
    """
    let time = (elapsed >> _shift).max(1)
    let pending =
      if time <= _mask() then
        let slot = time and _mask()
        let slots = (1 << slot) - 1
        let old_slot = _slot(current - elapsed)
        let new_slot = _slot(current)

        slots.rotl(old_slot) or
          slots.rotl(new_slot).rotr(slot) or
          (1 << new_slot)
      else
        -1
      end

    while (pending and _pending) != 0 do
      let slot = (pending and _pending).ctz()
      try list.append_list(_list(slot)) end
      _pending = _pending and not (1 << slot)
    end

    (pending and 1) != 0

  fun box next(current: U64): U64 =>
    """
    Given a current time, return the next time at which this timing wheel
    should be advanced. Returns -1 if no timers are on this timing wheel.
    """
    if _pending != 0 then
      let slot = _slot(current)
      let mask = (1 << _shift) - 1
      ((_pending.rotr(slot).ctz() + _adjust) << _shift) - (current and mask)
    else
      -1
    end

  fun ref clear() =>
    """
    Cancels all pending timers.
    """
    try
      for list in _list.values() do
        for timer in list.values() do
          timer._cancel()
        end
      end
    end

  fun box _slot(time: U64): U64 =>
    """
    Return the slot for a given time.
    """
    (time >> _shift) and _mask()

  fun tag _bits(): U64 => 6
  fun tag _max(): U64 => 1 << _bits()
  fun tag _mask(): U64 => _max() - 1
