// Adapted from https://github.com/miracle2k/python-glob2

use "regex"

primitive Glob
  """Filename matching and globbing with shell patterns.

  `fnmatch(file_name, pattern)` matches according to the local convention.
  `fnmatchcase(file_name, pattern)` always takes case in account.  The functions
  operate by translating the pattern into a regular expression.

  The function translate(PATTERN) returns a regular expression corresponding to PATTERN.

  """

  fun fnmatch(name: String, pattern: String): Bool =>
    """
    Tests whether `name` matches `pattern`.

    Patterns are Unix shell style:
        *       | matches files
        **      | matches files across directories
        ?       | matches any single character
        [seq]   | matches any character in seq
        [!seq]  | matches any char not in seq

    An initial period in `name` is not special.

    Both `name` and `pattern` are first case-normalized if the operating system
    requires it.  If you don't want this, use `fnmatchcase`.
    """
    fnmatchcase(Path.normcase(name), Path.normcase(pattern))

  fun fnmatchcase(name: String, pattern: String): Bool =>
      """Tests whether `name` matches `pattern`, including case."""
      try
        Regex(translate(pattern)) == name
      else
        false
      end

  fun filter(names: Array[String], pattern: String): Array[(String, Array[String])] val =>
      """
      Returns `name` and the matching subgroups for `names` that match `pattern`.

      All strings are first case-normalized if the operating system requires it.
      """
      let result = recover Array[(String, Array[String])] end
      try
        let regex = Regex(translate(Path.normcase(pattern)))
        for name in names.values() do
          try
            let m = regex(Path.normcase(name))
            result.push((name, m.groups()))
          end
        end
      end
      result

  fun translate(pat: String): String ref^ =>
    """
    Translates a shell `pattern` to a regular expression.
    There is no way to quote meta-characters.
    """
    let n = pat.size()
    let res = String(n)
    try
      let alpha_num_regex = Regex("\\w")
      var i: U64 = 0
      while i < n do
        let c = pat(i)
        i = i+1
        if c == '*' then
          if (i < n) and (pat(i) == '*') then
            res.append("(.*)")
            i = i + 1
          else
            res.append("([^/]*)")
          end
        elseif c == '?' then
          res.append("([^/])")
        elseif c == '[' then
          var j = i
          if (j < n) then
            if pat(j) == '!' then
              j = j + 1
            elseif pat(j) == ']' then
              j = j + 1
            end
          end
          while (j < n) and (pat(j) != ']') do
            j = j + 1
          end
          if j >= n then
            res.append("\\[")
          else
            res.append("([")
            if pat(i) == '!' then
              res.append("^")
              i = i + 1
            elseif pat(i) == '^' then
              res.append("\\^")
              i = i + 1
            end
            let sub = recover ref pat.substring(i.i64(), (j-1).i64()) end
            res.append(sub.replace("\\","\\\\"))
            res.append("])")
            i = j + 1
          end
        else
          if alpha_num_regex != String(1).push(c) then
            res.append("\\")
          end
          res.push(c)
        end
      end
      res.append("\\Z(?ms)")  // (?ms) turns on the multiline and dotall flags
    end
    res
