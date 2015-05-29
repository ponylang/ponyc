use "collections"

class Timer
  """
  A timer.
  """
  var _expiration: U64
  var _interval: U64
  let _notify: TimerNotify
  let _node: ListNode[Timer]

  new iso create(notify: TimerNotify iso, expiration: U64, interval: U64 = 0)
  =>
    """
    Create a new timer. The expiration time should be a nanosecond count
    until the first expiration. The interval should also be in nanoseconds.
    """
    _expiration = expiration + Time.nanos()
    _interval = interval
    _notify = consume notify
    _node = ListNode[Timer]
    _node() = this

  new abs(notify: TimerNotify, expiration: (I64, I64), interval: U64 = 0) =>
    """
    Creates a new timer with an absolute expiration time rather than a relative
    time. The expiration time is wall-clock adjusted system time.
    """
    _expiration = Time.wall_to_nanos(expiration)
    _interval = interval
    _notify = notify
    _node = ListNode[Timer]
    _node() = this

  fun ref _cancel() =>
    """
    Remove the timer from any list.
    """
    _node.remove()
    _notify.cancel(this)

  fun ref _get_node(): ListNode[Timer] =>
    """
    Returns the list node pointing to the timer. Used to schedule the timer in
    a queue.
    """
    _node

  fun ref _slop(bits: U64) =>
    """
    Apply slop bits to the expiration time and interval. This reduces the
    precision by the given number of bits, effectively quantizing time.
    """
    _expiration = _expiration >> bits

    if _interval > 0 then
      _interval = (_interval >> bits).max(1)
    end

  fun ref _fire(current: U64): Bool =>
    """
    A timer is fired if its expiration time is in the past. The notifier is
    called with a count based on the elapsed time since expiration and the
    timer interval. The expiration time is set to the next expiration. Returns
    true if the timer should be rescheduled, false otherwise.
    """
    let elapsed = current - _expiration

    if elapsed < (1 << 63) then
      let count = (elapsed / _interval) + 1
      _expiration = _expiration + (count * _interval)

      if not _notify(this, count) then
        _notify.cancel(this)
        return false
      end
    end

    (_interval > 0) or ((_expiration - current) < (1 << 63))

  fun _next(): U64 =>
    """
    Returns the next expiration time.
    """
    _expiration
