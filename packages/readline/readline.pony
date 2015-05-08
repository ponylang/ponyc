use "collections"
use "files"

class Readline is StdinNotify
  """
  Line editing, history, and tab completion.
  """
  let _notify: ReadlineNotify
  let _out: StdStream
  let _path: (String | None)
  let _history: Array[String]
  let _maxlen: U64

  var _edit: String iso = recover String end
  var _cur_prompt: String
  var _cur_line: U64 = 0
  var _cur_pos: I64 = 0
  var _escape: U8 = 0
  var _esc1: U8 = 0
  var _esc2: U8 = 0

  new iso create(notify: ReadlineNotify iso, out: StdStream,
    path: (String | None) = None, maxlen: U64 = 0)
  =>
    """
    Create a readline handler to be passed to stdin.
    """
    _notify = consume notify
    _out = out
    _path = path
    _history = Array[String](maxlen)
    _maxlen = maxlen

    _cur_prompt = try _notify("") else "> " end

    _load_history()
    _refresh_line()

  fun ref apply(data: Array[U8] iso): Bool =>
    """
    Receives input.
    """
    try
      var i = U64(0)

      while i < data.size() do
        var c = data(i)

        match _escape
        | 0 =>
          match c
          | 0x01 => _home() // ctrl-a
          | 0x02 => _left() // ctrl-b
          | 0x04 => _delete() // ctrl-d
          | 0x05 => _end() // ctrl-e
          | 0x06 => _right() // ctrl-f
          | 0x08 => _backspace() // ctrl-h
          | 0x09 => _tab()
          | 0x0A => _dispatch() // LF
          | 0x0B =>
            // ctrl-k, delete to the end of the line.
            _edit.truncate(_cur_pos.u64())
          | 0x0C => _clear() // ctrl-l
          | 0x0D => _dispatch() // CR
          | 0x0E => _down() // ctrl-n
          | 0x10 => _up() // ctrl-p
          | 0x14 => _swap() // ctrl-t
          | 0x15 =>
            // ctrl-u, delete the whole line.
            _edit.clear()
            _home()
          | 0x17 => _delete_prev_word() // ctrl-w
          | 0x1B => _escape = 1 // escape
          | 0x7F => _backspace() // backspace
          | where c < 0x20 => None // unknown control character
          else
            // Insert.
            _edit.insert_byte(_cur_pos, c)
            _cur_pos = _cur_pos + 1
          end
        | 1 =>
          // First escape byte.
          _esc1 = c
          _escape = 2
        | 2 =>
          // Second escape byte.
          _esc2 = c
          _escape = 0

          match _esc1
          | '[' =>
            match _esc2
            | 'A' => _up()
            | 'B' => _down()
            | 'C' => _right()
            | 'D' => _left()
            | 'H' => _home()
            | 'F' => _end()
            | where (_esc2 >= '0') and (_esc2 <= '9') =>
              // extended escape sequence
              _escape = 3
            end
          | '0' =>
            match _esc2
            | 'H' => _home()
            | 'F' => _end()
            end
          end
        | 3 =>
          _escape = 0

          match c
          | '~' =>
            match _esc2
            | '3' => _delete()
            end
          end
        end

        i = i + 1
      end

      _refresh_line()
      true
    else
      _save_history()
      false
    end

  fun ref closed() =>
    """
    No more input is available.
    """
    _save_history()

  fun ref _up() =>
    """
    Previous line.
    """
    try
      if _cur_line > 0 then
        _cur_line = _cur_line - 1
        _edit = _history(_cur_line).clone()
        _end()
      end
    end

  fun ref _down() =>
    """
    Next line.
    """
    try
      if _cur_line < (_history.size() - 1) then
        _cur_line = _cur_line + 1
        _edit = _history(_cur_line).clone()
      else
        _edit.clear()
      end

      _end()
    end

  fun ref _left() =>
    """
    Move left.
    """
    if _cur_pos == 0 then
      return
    end

    try
      repeat
        _cur_pos = _cur_pos - 1
      until
        (_cur_pos == 0) or
        ((_edit.at_offset(_cur_pos) and 0xC0) != 0x80)
      end
    end

  fun ref _right() =>
    """
    Move right.
    """
    try
      if _cur_pos < _edit.size().i64() then
        _cur_pos = _cur_pos + 1
      end

      while
        (_cur_pos < _edit.size().i64()) and
        ((_edit.at_offset(_cur_pos) and 0xC0) == 0x80)
      do
        _cur_pos = _cur_pos + 1
      end
    end

  fun ref _home() =>
    """
    Beginning of the line.
    """
    _cur_pos = 0

  fun ref _end() =>
    """
    End of the line.
    """
    _cur_pos = _edit.size().i64()

  fun ref _backspace() =>
    """
    Backward delete.
    """
    if _cur_pos == 0 then
      return
    end

    try
      var c = U8(0)

      repeat
        _cur_pos = _cur_pos - 1
        c = _edit.at_offset(_cur_pos)
        _edit.delete(_cur_pos, 1)
      until
        (_cur_pos == 0) or ((c and 0xC0) != 0x80)
      end
    end

  fun ref _delete() =>
    """
    Forward delete.
    """
    try
      if _cur_pos < _edit.size().i64() then
        _edit.delete(_cur_pos, 1)
      end

      while
        (_cur_pos < _edit.size().i64()) and
        ((_edit.at_offset(_cur_pos) and 0xC0) == 0x80)
      do
        _edit.delete(_cur_pos, 1)
      end
    end

  fun ref _clear() =>
    """
    Clear the screen.
    """
    _out.write("\x1b[H\x1b[2J")

  fun ref _swap() =>
    """
    Swap the previous character with the current one.
    """
    try
      if (_cur_pos > 0) and (_cur_pos < _edit.size().i64()) then
        _edit(_cur_pos.u64()) =
          _edit((_cur_pos - 1).u64()) =
            _edit(_cur_pos.u64())
      end
    end

  fun ref _delete_prev_word() =>
    """
    Delete the previous word.
    """
    try
      let old = _cur_pos

      while (_cur_pos > 0) and (_edit((_cur_pos - 1).u64()) == ' ') do
        _cur_pos = _cur_pos - 1
      end

      while (_cur_pos > 0) and (_edit((_cur_pos - 1).u64()) != ' ') do
        _cur_pos = _cur_pos - 1
      end

      _edit.delete(_cur_pos, (old - _cur_pos).u64())
    end

  fun ref _tab() =>
    """
    Tab completion.

    TODO: Improve this.
    """
    let r = _notify.tab(_edit.clone())

    match r.size()
    | 0 => None
    | 1 =>
      try
        _edit = r(0).clone()
        _end()
      end
    else
      _out.write("\n")

      for completion in r.values() do
        _out.print(completion)
      end
    end

  fun ref _dispatch() ? =>
    """
    Send a finished line to the notifier.
    """
    if _edit.size() > 0 then
      _home()
      _refresh_line()
      _out.write("\n")

      let line: String = _edit = recover String end
      _add_history(line)
      _cur_prompt = _notify(line)
    end

  fun ref _refresh_line() =>
    """
    Refresh the line on screen.
    """
    let len = 40 + _cur_prompt.size() + _edit.size()
    let out = recover String(len) end

    // Move to the left edge.
    out.append("\r")

    // Print the prompt.
    out.append(_cur_prompt)

    // Print the current line.
    out.append(_edit.clone())

    // Erase to the right edge.
    out.append("\x1B[0K")

    // Set the cursor position.
    var pos = _cur_prompt.codepoints()

    if _cur_pos > 0 then
      pos = pos + _edit.codepoints(0, _cur_pos - 1)
    end

    out.append("\r\x1B[")
    out.append(pos.string())
    out.append("C")

    _out.write(consume out)

  fun ref _add_history(line: String) =>
    """
    Add a line to the history, trimming an earlier line if necessary.
    """
    try
      if (_history.size() > 0) and (_history(_history.size() - 1) == line) then
        return
      end
    end

    if (_maxlen > 0) and (_history.size() >= _maxlen) then
      try
        _history.shift()
      end
    end

    _history.push(line)
    _cur_line = _history.size()

  fun ref _load_history() =>
    """
    Load the history from a file.
    """
    _history.clear()

    try
      with file = File.open(_path as String) do
        for line in file.lines() do
          _add_history(line)
        end
      end
    end

  fun _save_history() =>
    """
    Write the history back to a file.
    """
    try
      with file = File(_path as String) do
        for line in _history.values() do
          file.print(line)
        end
      end
    end
