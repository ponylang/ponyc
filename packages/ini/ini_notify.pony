primitive IniIncompleteSection
  """
  An `IniError` reported when a line that begins with `[` has no closing `]`.
  """

primitive IniNoDelimiter
  """
  An `IniError` reported when a line that is not a comment or section header
  contains neither `=` nor `:`.
  """

type IniError is
  ( IniIncompleteSection
  | IniNoDelimiter
  )
  """
  The set of errors `Ini` can report while parsing.
  """

interface IniNotify
  """
  Handler interface for the streaming parser `Ini`. The parser calls these
  methods as it works through the input. Only `apply` must be implemented;
  `add_section` and `errors` ship default implementations that accept all
  sections and continue past errors.
  """
  fun ref apply(section: String, key: String, value: String): Bool
    """
    Called for every key/value pair. Keys that appear before any section
    header are reported with `section` set to the empty string. Return
    `false` to halt processing.
    """

  fun ref add_section(section: String): Bool =>
    """
    Called for every `[section]` header. The implicit empty-string section
    that holds keys appearing before any header does not trigger this
    callback; only a literal header line (including `[]`) does. Return
    `false` to halt processing.
    """
    true

  fun ref errors(line: USize, err: IniError): Bool =>
    """
    Called for each malformed line. `line` is the 1-based line number where
    the error was detected. Return `false` to halt parsing immediately;
    `true` to keep going. Even when this returns `true` and parsing
    continues, `Ini.apply` will return `false` at the end.
    """
    true

primitive Ini
  """
  A streaming parser for INI formatted lines of text.

  Lines are pulled from the iterator one at a time and dispatched to an
  `IniNotify` handler as section headers, key/value pairs, or errors. The
  current section name is the only piece of parsed input that persists
  across handler calls; everything else (the line number reported with
  errors, the running success/failure status returned at the end) is
  internal bookkeeping the handler does not see.
  """
  fun apply(lines: Iterator[String box], f: IniNotify): Bool =>
    """
    Parse `lines` and call `f` for each section header and key/value pair.
    Returns `true` when parsing finishes with no errors, `false` otherwise.

    When the parser hits a malformed line it calls `f.errors`. If `errors`
    returns `true` parsing continues, but the final return value is still
    `false`. If `errors` returns `false` parsing stops immediately and
    `apply` returns `false`.

    `f.apply` and `f.add_section` can also stop parsing early by returning
    `false`. In that case `apply` returns whatever the parse status was at
    the point of the stop: `true` if no errors had been seen, `false` if any
    had.
    """
    var section = ""
    var lineno = USize(0)
    var ok = true

    for line in lines do
      lineno = lineno + 1
      var current = line.clone()
      current.strip()

      if current.size() == 0 then
        continue
      end

      try
        match current(0)?
        | ';' | '#' =>
          // Skip comments.
          continue
        | '[' =>
          try
            current.delete(current.find("]", 1)?, -1)
            current.delete(0)
            current.strip()
            section = consume current
            if not f.add_section(section) then
              return ok
            end
          else
            ok = false

            if not f.errors(lineno, IniIncompleteSection) then
              return false
            end
          end
        else
          try
            let delim =
              try
                current.find("=")?
              else
                current.find(":")?
              end

            let value = current.substring(delim + 1)
            value.strip()

            current.delete(delim, -1)
            current.strip()

            try
              let comment =
                try
                  value.find(";")?
                else
                  value.find("#")?
                end

              match value(comment.usize() - 1)?
              | ' ' | '\t' =>
                value.delete(comment, -1)
                value.rstrip()
              end
            end

            if not f(section, consume current, consume value) then
              return ok
            end
          else
            ok = false

            if not f.errors(lineno, IniNoDelimiter) then
              return false
            end
          end
        end
      end
    end
    ok
