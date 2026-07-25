use "constrained_types"
use "time"
use "signals"
use @ioctl[I32](fx: I32, cmd: ULong, ...) if posix

struct _TermSize
  var row: U16 = 0
  var col: U16 = 0
  var xpixel: U16 = 0
  var ypixel: U16 = 0

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

class _TermResizeNotify is SignalNotify
  let _term: ANSITerm tag

  new create(term: ANSITerm tag) =>
    _term = term

  fun apply(times: U32): Bool =>
    _term.size()
    true

primitive _TIOCGWINSZ
  fun apply(): ULong =>
    ifdef linux then
      21523
    elseif osx or bsd then
      1074295912
    elseif haiku then
      32780
    else
      0
    end

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
  var _esc_private: Bool = false
  embed _esc_buf: Array[U8] = Array[U8]
  var _closed: Bool = false
  let _auth: SignalAuth
  var _winch: (SignalHandler | None) = None

  new create(
    auth: SignalAuth,
    notify: ANSINotify iso,
    source: DisposableActor,
    timers: Timers = Timers)
  =>
    """
    Create a new ANSI term.
    """
    _timers = timers
    _notify = consume notify
    _source = source
    _auth = auth

    ifdef not windows then
      match MakeHandleableSignal(Sig.winch())
      | let sig: HandleableSignal =>
        _winch = SignalHandler(auth, recover _TermResizeNotify(this) end, sig)
      | let _: ValidationFailure =>
        // SIGWINCH is whitelisted on every platform where this branch
        // compiles; a rejection means the whitelist regressed.
        _Unreachable()
      end
    end

    _size()

  be apply(data: Array[U8] iso) =>
    """
    Receives input from stdin.
    """
    if _closed then
      return
    end

    for c in (consume data).values() do
      match \exhaustive\ _escape
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
          // ESC then some other byte. A control byte (a second ESC restarts) or
          // a high byte (a byte of a multi-byte character) is re-handled and
          // delivered; a printable ASCII byte is an unreportable Alt+<char>, so
          // it is discarded.
          if (c < 0x20) or (c >= 0x7F) then
            _abort_and_rehandle(c)
          else
            _esc_clear()
          end
        end
      | _EscapeSS3 =>
        if (c < 0x20) or (c >= 0x7F) then
          _abort_and_rehandle(c)
        else
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
            // Unrecognised SS3 final byte: discard the sequence.
            _esc_clear()
          end
        end
      | _EscapeCSI =>
        _csi(c, false)
      | _EscapeMod =>
        _csi(c, true)
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
    if _closed then
      return
    end

    _notify.prompt(this, value)

  be size() =>
    if _closed then
      return
    end

    _size()

  fun ref _size() =>
    """
    Pass the window size to the notifier.
    """
    let ws: _TermSize = _TermSize
    ifdef posix then
      @ioctl(0, _TIOCGWINSZ(), ws) // do error handling
      _notify.size(ws.row, ws.col)
    end

  be dispose() =>
    """
    Stop accepting input, inform the notifier we have closed, dispose of our
    source, and remove our terminal-resize handler.
    """
    if not _closed then
      _esc_clear()
      _notify.closed()
      _source.dispose()
      // Unregister the SIGWINCH handler installed in the constructor so it
      // stops delivering resize callbacks and frees its subscription.
      try (_winch as SignalHandler).dispose(_auth) end
      _closed = true
    end

  be _timeout() =>
    """
    Our timer since receiving an ESC has expired. Send the buffered data as if
    it was not an escape sequence. Guarded like the other notifier-forwarding
    behaviors so a timer that fires after dispose delivers nothing.
    """
    if _closed then
      return
    end

    _timer = None
    _esc_flush()

  fun ref _csi(c: U8, in_mod: Bool) =>
    """
    Handle one byte of a CSI sequence (ESC [ ...), reading parameter and
    intermediate bytes until a final byte ends the sequence. A recognised final
    byte dispatches its key; an unrecognised one, or one reached through a
    non-standard parameter, discards the sequence. `in_mod` is true once a `;`
    has been seen, so digits accumulate the modifier rather than the key number.
    """
    if (c >= '0') and (c <= '9') then
      // Accumulate a parameter, clamping on every digit so an oversized value
      // can't wrap a U8 into a different valid key or modifier.
      let d = (c - '0').u16()
      if in_mod then
        _esc_mod = (((_esc_mod.u16() * 10) + d).min(255)).u8()
      else
        _esc_num = (((_esc_num.u16() * 10) + d).min(255)).u8()
      end
      _esc_buf.push(c)
    elseif c == ';' then
      // A parameter separator. The first one moves us into modifier parameters;
      // any later ones are consumed as we read on to the final byte.
      if not in_mod then
        _escape = _EscapeMod
      end
      _esc_buf.push(c)
    elseif (c >= 0x20) and (c <= 0x3F) then
      // A parameter or intermediate byte we don't use (a private marker like
      // `?`, a sub-parameter `:`, an intermediate). Keep reading to the final
      // byte, but mark the sequence non-standard so it is discarded, not
      // dispatched as if it were a plain key.
      _esc_private = true
      _esc_buf.push(c)
    elseif (c >= 0x40) and (c <= 0x7E) then
      // A final byte ends the sequence.
      if _esc_private then
        _esc_clear()
      else
        match c
        | 'A' => _up()
        | 'B' => _down()
        | 'C' => _right()
        | 'D' => _left()
        | 'H' => _home()
        | 'F' => _end()
        | '~' => _keypad()
        else
          // Unrecognised final byte: discard the sequence.
          _esc_clear()
        end
      end
    else
      // Control byte (< 0x20 or >= 0x7F): abort.
      _abort_and_rehandle(c)
    end

  fun ref _abort_and_rehandle(c: U8) =>
    """
    Abort the escape sequence in progress. A new ESC starts a fresh sequence;
    any other byte is passed straight to the notifier, so a control key pressed
    mid-sequence still registers.
    """
    _esc_clear()
    if c == 0x1B then
      _escape = _EscapeStart
      _esc_buf.push(0x1B)
    else
      _notify(this, c)
    end

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
    else (false, false, false)
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
    else
      // Unrecognised keypad code: discard the sequence rather than leaving the
      // decoder stalled in its escape state.
      _esc_clear()
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
    Pass a partial escape sequence that timed out to the notifier as plain
    input.
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
    _esc_private = false
