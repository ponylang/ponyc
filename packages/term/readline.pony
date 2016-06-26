use "collections"
use "files"
use "promises"
use strings = "strings"

class Readline is ANSINotify
  """
  Line editing, history, and tab completion.
  """
  let _notify: ReadlineNotify
  let _out: OutStream
  let _path: (FilePath | None)
  embed _history: Array[String]
  embed _queue: Array[String] = Array[String]
  let _maxlen: USize

  var _edit: String iso = recover String end
  var _cur_prompt: String = ""
  var _cur_line: USize = 0
  var _cur_pos: ISize = 0
  var _blocked: Bool = true

  new iso create(notify: ReadlineNotify iso, out: OutStream,
    path: (FilePath | None) = None, maxlen: USize = 0)
  =>
    """
    Create a readline handler to be passed to stdin. It begins blocked. Set an
    initial prompt on the ANSITerm to begin processing.
    """
    _notify = consume notify
    _out = out
    _path = path
    _history = Array[String](maxlen)
    _maxlen = maxlen

    _load_history()

  fun ref apply(term: ANSITerm ref, input: U8) =>
    """
    Receives input.
    """
    match input
    | 0x01 => home() // ctrl-a
    | 0x02 => left() // ctrl-b
    | 0x04 => delete() // ctrl-d
    | 0x05 => end_key() // ctrl-e
    | 0x06 => right() // ctrl-f
    | 0x08 => _backspace() // ctrl-h
    | 0x09 => _tab()
    | 0x0A => _dispatch(term) // LF
    | 0x0B =>
      // ctrl-k, delete to the end of the line.
      _edit.truncate(_cur_pos.usize())
    | 0x0C => _clear() // ctrl-l
    | 0x0D => _dispatch(term) // CR
    | 0x0E => down() // ctrl-n
    | 0x10 => up() // ctrl-p
    | 0x14 => _swap() // ctrl-t
    | 0x15 =>
      // ctrl-u, delete the whole line.
      _edit.clear()
      home()
    | 0x17 => _delete_prev_word() // ctrl-w
    | 0x7F => _backspace() // backspace
    | if input < 0x20 => None // unknown control character
    else
      // Insert.
      _edit.insert_byte(_cur_pos, input)
      _cur_pos = _cur_pos + 1
      _refresh_line()
    end

  fun ref prompt(term: ANSITerm ref, value: String) =>
    """
    Set a new prompt, unblock, and handle the pending queue.
    """
    _cur_prompt = value
    _blocked = false

    try
      let line = _queue.shift()
      _add_history(line)
      _out.print(_cur_prompt + line)
      _handle_line(term, line)
    else
      _refresh_line()
    end

  fun ref closed() =>
    """
    No more input is available.
    """
    _save_history()

  fun ref up(ctrl: Bool = false, alt: Bool = false, shift: Bool = false) =>
    """
    Previous line.
    """
    try
      if _cur_line > 0 then
        _cur_line = _cur_line - 1
        _edit = _history(_cur_line).clone()
        end_key()
      end
    end

  fun ref down(ctrl: Bool = false, alt: Bool = false, shift: Bool = false) =>
    """
    Next line.
    """
    try
      if _cur_line < (_history.size() - 1) then
        _cur_line = _cur_line + 1
        _edit = _history(_cur_line).clone()
      else
        _cur_line = _history.size()
        _edit.clear()
      end

      end_key()
    end

  fun ref left(ctrl: Bool = false, alt: Bool = false, shift: Bool = false) =>
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

      _refresh_line()
    end

  fun ref right(ctrl: Bool = false, alt: Bool = false, shift: Bool = false) =>
    """
    Move right.
    """
    try
      if _cur_pos < _edit.size().isize() then
        _cur_pos = _cur_pos + 1
      end

      while
        (_cur_pos < _edit.size().isize()) and
        ((_edit.at_offset(_cur_pos) and 0xC0) == 0x80)
      do
        _cur_pos = _cur_pos + 1
      end

      _refresh_line()
    end

  fun ref home(ctrl: Bool = false, alt: Bool = false, shift: Bool = false) =>
    """
    Beginning of the line.
    """
    _cur_pos = 0
    _refresh_line()

  fun ref end_key(ctrl: Bool = false, alt: Bool = false, shift: Bool = false)
  =>
    """
    End of the line.
    """
    _cur_pos = _edit.size().isize()
    _refresh_line()

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

      _refresh_line()
    end

  fun ref delete(ctrl: Bool = false, alt: Bool = false, shift: Bool = false) =>
    """
    Forward delete.
    """
    try
      if _cur_pos < _edit.size().isize() then
        _edit.delete(_cur_pos, 1)
      end

      while
        (_cur_pos < _edit.size().isize()) and
        ((_edit.at_offset(_cur_pos) and 0xC0) == 0x80)
      do
        _edit.delete(_cur_pos, 1)
      end

      _refresh_line()
    end

  fun ref _clear() =>
    """
    Clear the screen.
    """
    _out.write(ANSI.clear())
    _refresh_line()

  fun ref _swap() =>
    """
    Swap the previous character with the current one.
    """
    try
      if (_cur_pos > 0) and (_cur_pos < _edit.size().isize()) then
        _edit(_cur_pos.usize()) =
          _edit((_cur_pos - 1).usize()) =
            _edit(_cur_pos.usize())
      end

      _refresh_line()
    end

  fun ref _delete_prev_word() =>
    """
    Delete the previous word.
    """
    try
      let old = _cur_pos

      while (_cur_pos > 0) and (_edit((_cur_pos - 1).usize()) == ' ') do
        _cur_pos = _cur_pos - 1
      end

      while (_cur_pos > 0) and (_edit((_cur_pos - 1).usize()) != ' ') do
        _cur_pos = _cur_pos - 1
      end

      _edit.delete(_cur_pos, (old - _cur_pos).usize())
      _refresh_line()
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
        end_key()
      end
    else
      _out.write("\n")

      for completion in r.values() do
        _out.print(completion)
      end

      _edit = strings.CommonPrefix(r)
      end_key()
    end

  fun ref _dispatch(term: ANSITerm) =>
    """
    Send a finished line to the notifier.
    """
    if _edit.size() > 0 then
      let line: String = _edit = recover String end

      if _blocked then
        _queue.push(line)
      else
        _add_history(line)
        _out.write("\n")
        _handle_line(term, line)
      end
    end

  fun ref _handle_line(term: ANSITerm, line: String) =>
    """
    Dispatch a single line.
    """
    let promise = Promise[String]

    promise.next[Any tag](
      recover term~prompt() end,
      recover term~dispose() end
      )

    _notify(line, promise)
    _cur_pos = 0
    _blocked = true

  fun ref _refresh_line() =>
    """
    Refresh the line on screen.
    """
    if not _blocked then
      let len = 40 + _cur_prompt.size() + _edit.size()
      let out = recover String(len) end

      // Move to the left edge.
      out.append("\r")

      // Print the prompt.
      out.append(_cur_prompt)

      // Print the current line.
      out.append(_edit.clone())

      // Erase to the right edge.
      out.append(ANSI.erase())

      // Set the cursor position.
      var pos = _cur_prompt.codepoints()

      if _cur_pos > 0 then
        pos = pos + _edit.codepoints(0, _cur_pos)
      end

      out.append("\r")
      out.append(ANSI.right(pos.u32()))
      _out.write(consume out)
    end

  fun ref _add_history(line: String) =>
    """
    Add a line to the history, trimming an earlier line if necessary.
    """
    try
      if (_history.size() > 0) and (_history(_history.size() - 1) == line) then
        _cur_line = _history.size()
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
      with file = File.open(_path as FilePath) do
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
      with file = File(_path as FilePath) do
        for line in _history.values() do
          file.print(line)
        end
      end
    end
