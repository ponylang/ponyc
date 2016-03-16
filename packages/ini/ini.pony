primitive IniIncompleteSection
primitive IniNoDelimeter

type IniError is
  ( IniIncompleteSection
  | IniNoDelimeter
  )

interface IniNotify
  """
  Notifications for INI parsing.
  """
  fun ref apply(section: String, key: String, value: String): Bool
    """
    This is called for every valid entry in the INI file. If key/value pairs
    occur before a section name, the section can be an empty string. Return
    false to halt processing.
    """

  fun ref errors(line: USize, err: IniError): Bool =>
    """
    This is called for each error encountered. Return false to halt processing.
    """
    true

primitive Ini
  """
  A streaming parser for INI formatted lines of test.
  """
  fun apply(lines: Iterator[String box], f: IniNotify): Bool =>
    """
    This accepts a string iterator and calls the IniNotify for each new entry.
    Empty sections won't be reported. If any errors are encountered, this will
    return false. Otherwise, it returns true.
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
        match current(0)
        | ';' | '#' =>
          // Skip comments.
          continue
        | '[' =>
          try
            current.delete(current.find("]", 1), -1)
            current.delete(0)
            section = consume current
          else
            ok = false

            if not f.errors(lineno, IniIncompleteSection) then
              return false
            end
          end
        else
          try
            let delim = try
              current.find("=")
            else
              current.find(":")
            end

            let value = current.substring(delim + 1)
            value.strip()

            current.delete(delim, -1)
            current.strip()

            try
              let comment = try
                value.find(";")
              else
                value.find("#")
              end

              match value(comment.usize() - 1)
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

            if not f.errors(lineno, IniNoDelimeter) then
              return false
            end
          end
        end
      end
    end
    ok
