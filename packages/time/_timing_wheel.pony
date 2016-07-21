use "collections"

class _TimingWheel
  """
  A timing wheel in a hierarchical set of timing wheels. Each wheel covers 6
  bits of precision.
  """
  let _shift: U64
  let _adjust: U64
  var _pending: U64 = 0
  embed _list: Array[List[Timer]]

  new create(index: USize) =>
    """
    Create a timing wheel at the given hierarchical level.
    """
    _shift = (index * _bits()).u64()
    _adjust = if index > 0 then 1 else 0 end
    _list = Array[List[Timer]](_max())

    for i in Range(0, _max()) do
      _list.push(List[Timer])
    end

  fun ref schedule(timer: Timer) =>
    """
    Schedule a timer on this wheel. Mark the bit indicating that the given slot
    has timers in its list.
    """
    let slot = ((timer._next() >> _shift) - _adjust) and _mask()

    try
      let list = _list(slot.usize())
      _list(slot.usize()).append_node(timer._get_node())
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
      try list.append_list(_list(slot.usize())) end
      _pending = _pending and not (1 << slot)
    end

    (pending and 1) != 0

  fun next(current: U64): U64 =>
    """
    Given a current time, return the next time at which this timing wheel
    should be advanced. Returns -1 if no timers are on this timing wheel.
    """
    if _pending != 0 then
      let slot = _slot(current)
      let mask = (1 << _shift) - 1
      ((_pending.rotr(slot).ctz() + _adjust).u64() << _shift.u64()) -
        (current and mask)
    else
      -1
    end

  fun ref clear() =>
    """
    Cancels all pending timers.
    """
    for list in _list.values() do
      for timer in list.values() do
        timer._cancel()
      end
    end

  fun _slot(time: U64): U64 =>
    """
    Return the slot for a given time.
    """
    (time >> _shift) and _mask()

  fun tag _bits(): USize => 6
  fun tag _max(): USize => 1 << _bits()
  fun tag _mask(): U64 => (_max() - 1).u64()
