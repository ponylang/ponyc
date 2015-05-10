class ANSITerm is StdinNotify
  """
  Handles ANSI escape codes from stdin.
  """
  let _notify: ANSINotify
  var _escape: U8 = 0
  var _esc_num: U8 = 0
  var _esc_mod: U8 = 0
  var _ctrl: Bool = false
  var _alt: Bool = false
  var _shift: Bool = false

  new iso create(notify: ANSINotify iso) =>
    """
    Create a new ANSI term.
    """
    _notify = consume notify

  fun ref apply(data: Array[U8] iso): Bool =>
    """
    Receives input from stdin.
    """
    try
      var i = U64(0)

      while i < data.size() do
        var c = data(i)

        match _escape
        | 0 =>
          if c == 0x1B then
            _escape = 1
          else
            if not _notify(c) then
              return false
            end
          end
        | 1 =>
          // Escape byte.
          _escape = 0

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
            // SS3
            _escape = 2
          | '[' =>
            // CSI
            _escape = 3
            _esc_num = 0
            _esc_mod = 0
          end
        | 2 =>
          // SS3
          _escape = 0

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
          end
        | 3 =>
          // CSI
          _escape = 0

          match c
          | 'A' => _up()
          | 'B' => _down()
          | 'C' => _right()
          | 'D' => _left()
          | 'H' => _home()
          | 'F' => _end()
          | '~' => _keypad()
          | ';' =>
            // Escape modifier.
            _escape = 4
          | where (c >= '0') and (c <= '9') =>
            // Escape number.
            _escape = 3
            _esc_num = (_esc_num * 10) + (c - '0')
          end
        | 4 =>
          // CSI modifier.
          _escape = 0

          match c
          | 'A' => _up()
          | 'B' => _down()
          | 'C' => _right()
          | 'D' => _left()
          | 'H' => _home()
          | 'F' => _end()
          | '~' => _keypad()
          | where (c >= '0') and (c <= '9') =>
            // Escape modifier.
            _escape = 4
            _esc_mod = (_esc_mod * 10) + (c - '0')
          end
        end

        i = i + 1
      end

      true
    else
      _notify.closed()
      false
    end

  fun ref _mod() =>
    """
    Set the modifier bools.
    """
    (_ctrl, _alt, _shift) = match _esc_mod
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
    _mod()
    _notify.up(_ctrl, _alt, _shift)

  fun ref _down() =>
    """
    Down arrow.
    """
    _mod()
    _notify.down(_ctrl, _alt, _shift)

  fun ref _left() =>
    """
    Left arrow.
    """
    _mod()
    _notify.left(_ctrl, _alt, _shift)

  fun ref _right() =>
    """
    Right arrow.
    """
    _mod()
    _notify.right(_ctrl, _alt, _shift)

  fun ref _delete() =>
    """
    Delete key.
    """
    _mod()
    _notify.delete(_ctrl, _alt, _shift)

  fun ref _insert() =>
    """
    Insert key.
    """
    _mod()
    _notify.insert(_ctrl, _alt, _shift)

  fun ref _home() =>
    """
    Home key.
    """
    _mod()
    _notify.home(_ctrl, _alt, _shift)

  fun ref _end() =>
    """
    End key.
    """
    _mod()
    _notify.end_key(_ctrl, _alt, _shift)

  fun ref _page_up() =>
    """
    Page up key.
    """
    _mod()
    _notify.page_up(_ctrl, _alt, _shift)

  fun ref _page_down() =>
    """
    Page down key.
    """
    _mod()
    _notify.page_down(_ctrl, _alt, _shift)

  fun ref _fn_key(i: U8) =>
    """
    Function key.
    """
    _mod()
    _notify.fn_key(i, _ctrl, _alt, _shift)
