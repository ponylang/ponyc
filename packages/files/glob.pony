// Adapted from https://github.com/miracle2k/python-glob2

use "regex"

interface GlobHandler
  """
  A handler for `Glob.iglob`.  Each path which matches the glob will be called
  with the groups that matched the various wildcards supplies in the
  `match_groups` array.
  """
  fun ref apply(path: FilePath, match_groups: Array[String])

primitive Glob
  """
  Filename matching and globbing with shell patterns.

  `fnmatch(file_name, pattern)` matches according to the local convention.
  `fnmatchcase(file_name, pattern)` always takes case into account.  The
  functions operate by translating the pattern into a regular expression.

  The function translate(PATTERN) returns a regular expression corresponding to
  PATTERN.

  Patterns are Unix shell style:
      *       | matches multiple characters within a directory
      **      | matches multiple characters across directories
      ?       | matches any single character
      [seq]   | matches any character in seq
      [!seq]  | matches any char not in seq
  """

  fun fnmatch(name: String, pattern: String): Bool =>
    """
    Tests whether `name` matches `pattern`.

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

  fun filter(names: Array[String], pattern: String):
    Array[(String, Array[String])] val
  =>
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
    res.append("\\A")  // start of string
    try
      let alpha_num_regex = Regex("\\w")
      var i: USize = 0
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
            let sub = recover ref pat.substring(i.isize(), j.isize()) end
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
      res.append("\\Z(?ms)")  // end of string + (?ms) multiline and dotall flags
    end
    res

  fun glob(root_path: FilePath, pattern: String): Array[FilePath] =>
    """
    Returns an Array[FilePath] for each path below `root_path` that matches
    `pattern`.

    The pattern may contain shell-style wildcards.  See the type documentation
    on `Glob` for details.
    """
    let res = recover ref Array[FilePath] end
    iglob(
      root_path, pattern,
      lambda ref(path: FilePath, match_groups: Array[String])(res) =>
        res.push(path)
      end)
    res

  fun _apply_glob_to_walk(
    pattern: String, compiled_pattern: Regex, root: FilePath,
    glob_handler: GlobHandler ref, dir: FilePath, entries: Array[String] ref)
  =>
    for e in entries.values() do
      try 
        let p = dir.join(e)
        let m = compiled_pattern(
          if Path.is_abs(pattern) then
            p.path
          else
            Path.rel(root.path, p.path)
          end)
        glob_handler(p, m.groups())
      end 
    end

  fun iglob(root: FilePath, pattern: String, glob_handler: GlobHandler ref) =>
    """
    Calls `GlobHandler.apply` for each path below `root` that matches `pattern`.

    The pattern may contain shell-style wildcards.  See the type documentation
    on `Glob` for details.
    """
    // TODO: do something efficient by looking at parts of the pattern that do
    // not contain wildcards and expanding them before walking.
    try
      root.walk(this~_apply_glob_to_walk(
        pattern , Regex(translate(Path.normcase(pattern))), root, glob_handler))
    end

