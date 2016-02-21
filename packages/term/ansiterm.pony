use "time"

primitive _EscapeNone
primitive _EscapeStart
primitive _EscapeSS3
primitive _EscapeCSI
primitive _EscapeMod

type _EscapeState is
  ( _EscapeNone
  | _EscapeStart
  | _EscapeSS3
  | _EscapeCSI
  | _EscapeMod
  )

actor ANSITerm
  """
  Handles ANSI escape codes from stdin.
  """
  let _timers: Timers
  var _timer: (Timer tag | None) = None
  let _notify: ANSINotify
  let _source: DisposableActor
  var _escape: _EscapeState = _EscapeNone
  var _esc_num: U8 = 0
  var _esc_mod: U8 = 0
  let _esc_buf: Array[U8] = Array[U8]
  var _closed: Bool = false

  new create(notify: ANSINotify iso, source: DisposableActor,
    timers: Timers = Timers)
  =>
    """
    Create a new ANSI term.
    """
    _timers = timers
    _notify = consume notify
    _source = source

  be apply(data: Array[U8] iso) =>
    """
    Receives input from stdin.
    """
    if _closed then
      return
    end

    for c in (consume data).values() do
      match _escape
      | _EscapeNone =>
        if c == 0x1B then
          _escape = _EscapeStart
          _esc_buf.push(0x1B)
        else
          _notify(this, c)
        end
      | _EscapeStart =>
        match c
        | 'b' =>
          // alt-left
          _esc_mod = 3
          _left()
        | 'f' =>
          // alt-right
          _esc_mod = 3
          _right()
        | 'O' =>
          _escape = _EscapeSS3
          _esc_buf.push(c)
        | '[' =>
          _escape = _EscapeCSI
          _esc_buf.push(c)
        else
          _esc_flush()
        end
      | _EscapeSS3 =>
        match c
        | 'A' => _up()
        | 'B' => _down()
        | 'C' => _right()
        | 'D' => _left()
        | 'H' => _home()
        | 'F' => _end()
        | 'P' => _fn_key(1)
        | 'Q' => _fn_key(2)
        | 'R' => _fn_key(3)
        | 'S' => _fn_key(4)
        else
          _esc_flush()
        end
      | _EscapeCSI =>
        match c
        | 'A' => _up()
        | 'B' => _down()
        | 'C' => _right()
        | 'D' => _left()
        | 'H' => _home()
        | 'F' => _end()
        | '~' => _keypad()
        | ';' =>
          _escape = _EscapeMod
        | if (c >= '0') and (c <= '9') =>
          // Escape number.
          _esc_num = (_esc_num * 10) + (c - '0')
          _esc_buf.push(c)
        else
          _esc_flush()
        end
      | _EscapeMod =>
        match c
        | 'A' => _up()
        | 'B' => _down()
        | 'C' => _right()
        | 'D' => _left()
        | 'H' => _home()
        | 'F' => _end()
        | '~' => _keypad()
        | if (c >= '0') and (c <= '9') =>
          // Escape modifier.
          _esc_mod = (_esc_mod * 10) + (c - '0')
          _esc_buf.push(c)
        else
          _esc_flush()
        end
      end
    end

    // If we are in the middle of an escape sequence, set a timer for 25 ms.
    // If it fires, we send the escape sequence as if it was normal data.
    if _escape isnt _EscapeNone then
      if _timer isnt None then
        try _timers.cancel(_timer as Timer tag) end
      end

      let t = recover
        object is TimerNotify
          let term: ANSITerm = this

          fun ref apply(timer: Timer, count: U64): Bool =>
            term._timeout()
            false
        end
      end

      let timer = Timer(consume t, 25000000)
      _timer = timer
      _timers(consume timer)
    end

  be prompt(value: String) =>
    """
    Pass a prompt along to the notifier.
    """
    _notify.prompt(this, value)

  be dispose() =>
    """
    Stop accepting input, inform the notifier we have closed, and dispose of
    our source.
    """
    if not _closed then
      _esc_clear()
      _notify.closed()
      _source.dispose()
      _closed = true
    end

  be _timeout() =>
    """
    Our timer since receiving an ESC has expired. Send the buffered data as if
    it was not an escape sequence.
    """
    _timer = None
    _esc_flush()

  fun ref _mod(): (Bool, Bool, Bool) =>
    """
    Set the modifier bools.
    """
    let r = match _esc_mod
    | 2 => (false, false, true)
    | 3 => (false, true, false)
    | 4 => (false, true, true)
    | 5 => (true, false, false)
    | 6 => (true, false, true)
    | 7 => (true, true, false)
    | 8 => (true, true, true)
    else
      (false, false, false)
    end

    _esc_mod = 0
    r

  fun ref _keypad() =>
    """
    An extended key.
    """
    match _esc_num
    | 1 => _home()
    | 2 => _insert()
    | 3 => _delete()
    | 4 => _end()
    | 5 => _page_up()
    | 6 => _page_down()
    | 11 => _fn_key(1)
    | 12 => _fn_key(2)
    | 13 => _fn_key(3)
    | 14 => _fn_key(4)
    | 15 => _fn_key(5)
    | 17 => _fn_key(6)
    | 18 => _fn_key(7)
    | 19 => _fn_key(8)
    | 20 => _fn_key(9)
    | 21 => _fn_key(10)
    | 23 => _fn_key(11)
    | 24 => _fn_key(12)
    | 25 => _fn_key(13)
    | 26 => _fn_key(14)
    | 28 => _fn_key(15)
    | 29 => _fn_key(16)
    | 31 => _fn_key(17)
    | 32 => _fn_key(18)
    | 33 => _fn_key(19)
    | 34 => _fn_key(20)
    end

  fun ref _up() =>
    """
    Up arrow.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.up(ctrl, alt, shift)
    _esc_clear()

  fun ref _down() =>
    """
    Down arrow.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.down(ctrl, alt, shift)
    _esc_clear()

  fun ref _left() =>
    """
    Left arrow.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.left(ctrl, alt, shift)
    _esc_clear()

  fun ref _right() =>
    """
    Right arrow.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.right(ctrl, alt, shift)
    _esc_clear()

  fun ref _delete() =>
    """
    Delete key.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.delete(ctrl, alt, shift)
    _esc_clear()

  fun ref _insert() =>
    """
    Insert key.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.insert(ctrl, alt, shift)
    _esc_clear()

  fun ref _home() =>
    """
    Home key.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.home(ctrl, alt, shift)
    _esc_clear()

  fun ref _end() =>
    """
    End key.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.end_key(ctrl, alt, shift)
    _esc_clear()

  fun ref _page_up() =>
    """
    Page up key.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.page_up(ctrl, alt, shift)
    _esc_clear()

  fun ref _page_down() =>
    """
    Page down key.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.page_down(ctrl, alt, shift)
    _esc_clear()

  fun ref _fn_key(i: U8) =>
    """
    Function key.
    """
    (let ctrl, let alt, let shift) = _mod()
    _notify.fn_key(i, ctrl, alt, shift)
    _esc_clear()

  fun ref _esc_flush() =>
    """
    Pass a partial or unrecognised escape sequence to the notifier.
    """
    for c in _esc_buf.values() do
      _notify(this, c)
    end

    _esc_clear()

  fun ref _esc_clear() =>
    """
    Clear the escape state.
    """
    if _timer isnt None then
      try _timers.cancel(_timer as Timer tag) end
      _timer = None
    end

    _escape = _EscapeNone
    _esc_buf.clear()
    _esc_num = 0
    _esc_mod = 0
