use "collections"

class Timer
  """
  The `Timer` class represents a timer that fires after an expiration
  time, and then fires at an interval. When a `Timer` fires, it calls
  the `apply` method of the `TimerNotify` object that was passed to it
  when it was created.

  The following example waits 5 seconds and then fires every 2
  seconds, and when it fires the `TimerNotify` object prints how many
  times it has been called:

  ```
  use "time"

  actor Main
    new create(env: Env) =>
      let timers = Timers
      let timer = Timer(Notify(env), 5_000_000_000, 2_000_000_000)
      timers(consume timer)

  class Notify is TimerNotify
    let _env: Env
    var _counter: U32 = 0
    new iso create(env: Env) =>
      _env = env
    fun ref apply(timer: Timer, count: U64): Bool =>
      _env.out.print(_counter.string())
      _counter = _counter + 1
      true
  ```
  """
  var _expiration: U64
  var _interval: U64
  let _notify: TimerNotify
  embed _node: ListNode[Timer]

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
    try _node() = this end

  new abs(notify: TimerNotify, expiration: (I64, I64), interval: U64 = 0) =>
    """
    Creates a new timer with an absolute expiration time rather than a relative
    time. The expiration time is wall-clock adjusted system time.
    """
    _expiration = Time.wall_to_nanos(expiration)
    _interval = interval
    _notify = notify
    _node = ListNode[Timer]
    try _node() = this end

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

  fun ref _slop(bits: USize) =>
    """
    Apply slop bits to the expiration time and interval. This reduces the
    precision by the given number of bits, effectively quantizing time.
    """
    _expiration = _expiration >> bits.u64()

    if _interval > 0 then
      _interval = (_interval >> bits.u64()).max(1)
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
