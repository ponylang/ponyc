use "collections"
use "files"

class val _CompileError
  """
  One compile error: its headline and its Info lines, each written as
  `path:line:col: message` with paths relative to the test directory.
  """
  let headline: String
  let infos: Array[String] val

  new val create(headline': String, infos': Array[String] val) =>
    headline = headline'
    infos = infos'

  fun string(): String =>
    let s = recover iso String end
    s.append(headline)
    for info in infos.values() do
      s.append("\n    ")
      s.append(info)
    end
    consume s

class val _ExpectedErrors
  """
  The errors a must-fail test expects, read from its
  `expected-compile-errors.txt`, in the order written. There is at least one.
  """
  let errors: Array[_CompileError] val

  new val create(errors': Array[_CompileError] val) ? =>
    if errors'.size() == 0 then error end
    errors = errors'

primitive _CompileErrors
  """
  Reading ponyc's error output and the expected-errors file, and comparing the
  two.

  ponyc prints each error as a line `Error:`, a headline `file:line:col:
  message` (or a shorter form when there is no line), and, when the source is
  known, the source line and a caret line. An error with Info frames then
  prints `    Info:` and each frame as a four-space-indented location line,
  with its own indented source and caret lines.
  """

  fun parse_expected(text: String): (_ExpectedErrors | String) =>
    """
    The expected errors, or a reason the file can't be used. A line with no
    indent is a headline; a line indented by four spaces is an Info line of the
    headline above it.
    """
    let lines = _lines(text)
    let errors = recover iso Array[_CompileError] end
    var headline: (String | None) = None
    var infos = recover iso Array[String] end

    for line in lines.values() do
      if line.size() == 0 then continue end

      if line.at("    ") then
        if headline is None then
          return "an Info line comes before any headline: '" + line + "'"
        end
        let entry: String val = line.substring(4)
        infos.push(_normalize_location(entry, ""))
      else
        match headline
        | let h: String =>
          errors.push(_CompileError(h, infos = recover iso Array[String] end))
        end
        headline = _normalize_location(line, "")
      end
    end

    match headline
    | let h: String =>
      errors.push(_CompileError(h, infos = recover iso Array[String] end))
    end

    try
      _ExpectedErrors(consume errors)?
    else
      "expected-compile-errors.txt lists no errors"
    end

  fun parse_actual(stderr: String, test_dir: String)
    : Array[_CompileError] val
  =>
    """
    The errors in ponyc's stderr, with the canonical test directory
    `test_dir` stripped from their paths. Source and caret lines are skipped.
    """
    let prefix = _dir_prefix(test_dir)
    let errors = recover iso Array[_CompileError] end
    var headline: (String | None) = None
    var infos = recover iso Array[String] end
    var expect_headline = false
    var in_info = false

    for line in _lines(stderr).values() do
      if expect_headline then
        expect_headline = false
        headline = _normalize_location(line, prefix)
        continue
      end

      if line == "Error:" then
        match headline
        | let h: String =>
          errors.push(_CompileError(h, infos = recover iso Array[String] end))
        end
        headline = None
        expect_headline = true
        in_info = false
      elseif line == "    Info:" then
        in_info = true
      elseif in_info and line.at("    ") then
        let entry: String val = line.substring(4)
        if _split_location(entry) isnt None then
          infos.push(_normalize_location(entry, prefix))
        end
      end
    end

    match headline
    | let h: String =>
      errors.push(_CompileError(h, infos = recover iso Array[String] end))
    end

    consume errors

  fun compare(
    expected: _ExpectedErrors,
    actual: Array[_CompileError] val)
    : (None | String)
  =>
    """
    None when the actual errors match the expected ones, otherwise a
    description of the difference.
    """
    if expected.errors.size() != actual.size() then
      return "expected " + expected.errors.size().string() +
        " error(s); the compiler reported " + actual.size().string() +
        "\n" + _describe(expected.errors, actual)
    end

    for (i, e) in expected.errors.pairs() do
      try
        match _error_mismatch(e, actual(i)?)
        | let reason: String =>
          return "error " + (i + 1).string() + ": " + reason + "\n" +
            _describe(expected.errors, actual)
        end
      else
        _Unreachable()
      end
    end
    None

  fun _error_mismatch(expected: _CompileError, actual: _CompileError)
    : (None | String)
  =>
    """
    None when `actual` matches `expected`: equal headlines, the expected Info
    lines found in the actual ones in the same order, and every copy note or
    privacy blame note in the actual error listed in the expected one.
    Otherwise the reason they differ.
    """
    if expected.headline != actual.headline then
      return "headline differs"
    end

    let matched = Array[Bool].init(false, actual.infos.size())
    var next: USize = 0
    for info in expected.infos.values() do
      var found = false
      while next < actual.infos.size() do
        try
          if actual.infos(next)? == info then
            matched(next)? = true
            found = true
            next = next + 1
            break
          end
        else
          _Unreachable()
        end
        next = next + 1
      end
      if not found then
        return "Info line missing or out of order: '" + info + "'"
      end
    end

    for (i, info) in actual.infos.pairs() do
      if (not (try matched(i)? else _Unreachable(); false end)) and
        _is_note(info)
      then
        return "unlisted note: '" + info + "'"
      end
    end
    None

  fun _is_note(info: String): Bool =>
    """
    Whether an Info line is a note about inherited code: the note naming the
    method and the types it was inherited through, or a note saying how a
    private type reached the code. A test must list these; other Info lines
    may be left out.
    """
    let message =
      match _split_location(info)
      | (_, let rest: String) => rest
      else
        info
      end
    // The same words as COPY_NOTE_START, COPY_NOTE_INHERITED and
    // BLAME_NOTE_REACHES in src/libponyc/ast/error.h, which the compiler
    // builds the notes with. If those change and these don't, tests stop
    // having to list the notes.
    (message.at("in method '") and message.contains("' inherited by ")) or
      message.contains("' and reaches this code through ")

  fun _describe(
    expected: Array[_CompileError] val,
    actual: Array[_CompileError] val)
    : String
  =>
    let s = recover iso String end
    s.append("expected:\n")
    for e in expected.values() do
      s.append("  ")
      s.append(e.string())
      s.append("\n")
    end
    s.append("actual:\n")
    for a in actual.values() do
      s.append("  ")
      s.append(a.string())
      s.append("\n")
    end
    consume s

  fun _lines(text: String): Array[String] val =>
    """
    The lines of `text`, each without a trailing carriage return.
    """
    recover val
      let lines = Array[String]
      for line in text.split_by("\n").values() do
        let l = line.clone()
        if (l.size() > 0) and (try l(l.size() - 1)? == '\r' else false end)
        then
          l.truncate(l.size() - 1)
        end
        lines.push(consume l)
      end
      lines
    end

  fun _dir_prefix(test_dir: String): String =>
    """
    The test directory as it appears at the start of a path in ponyc's output:
    with `/` separators and a trailing `/`.
    """
    let p = recover iso test_dir.clone() end
    p.replace("\\", "/")
    if not p.at("/", -1) then
      p.append("/")
    end
    _fold_case(consume p)

  fun _char_at(line: String, i: ISize): U8 =>
    """
    The byte at `i`, which the caller has checked is in `line`.
    """
    try line(i.usize())? else _Unreachable(); 0 end

  fun _split_location(line: String): ((String, String) | None) =>
    """
    The path and the message of a line of the form `path:line:col: message`,
    with `:line:col` kept on the path. The split is at the first `: ` that has
    `:line:col` just before it, so a Windows drive letter stays in the path.
    None when the line has no such form.
    """
    var start: ISize = 0
    while true do
      let pos = try line.find(": ", start)? else return None end
      // Walk back over `col`, `:`, `line` and the `:` before the line.
      var i = pos - 1
      var digits: USize = 0
      while (i >= 0) and _is_digit(_char_at(line, i)) do
        i = i - 1
        digits = digits + 1
      end
      if (digits > 0) and (i > 0) and
        (_char_at(line, i) == ':')
      then
        i = i - 1
        digits = 0
        while (i >= 0) and _is_digit(_char_at(line, i)) do
          i = i - 1
          digits = digits + 1
        end
        if (digits > 0) and (i > 0) and
          (_char_at(line, i) == ':')
        then
          return (line.substring(0, pos), line.substring(pos + 2))
        end
      end
      start = pos + 1
    end
    None

  fun _is_digit(c: U8): Bool =>
    (c >= '0') and (c <= '9')

  fun _normalize_location(line: String, prefix: String): String =>
    """
    `line` with the path part rewritten to `/` separators and the test
    directory prefix removed. On Windows the path part is lower-cased, because
    paths there compare without case. A line that is not a location line is
    returned with the prefix removed if it starts with it.
    """
    match _split_location(line)
    | (let path: String, let message: String) =>
      let p = recover iso path.clone() end
      p.replace("\\", "/")
      let folded = _fold_case(consume p)
      let relative =
        if (prefix.size() > 0) and folded.at(prefix) then
          folded.substring(prefix.size().isize())
        else
          folded
        end
      relative + ": " + message
    else
      if (prefix.size() > 0) and _fold_case(line).at(prefix) then
        line.substring(prefix.size().isize())
      else
        line
      end
    end

  fun _fold_case(s: String): String =>
    ifdef windows then s.lower() else s end
