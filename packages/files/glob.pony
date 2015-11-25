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

  fun has_magic(pattern: String): Bool =>
    """
    Returns true if a pattern has magic characters.
    """
    try Regex("[*?[]") == pattern else true end

  fun _is_hidden(path: String): Bool =>
    try path(0) == '.' else false end

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

//     def glob(self, pathname, with_matches=False, include_hidden=False):
//         """Return a list of paths matching a pathname pattern.
//         The pattern may contain simple shell-style wildcards a la
//         fnmatch. However, unlike fnmatch, filenames starting with a
//         dot are special cases that are not matched by '*' and '?'
//         patterns.
//         If ``include_hidden`` is True, then files and folders starting with
//         a dot are also returned.
//         """
//         return list(self.iglob(pathname, with_matches, include_hidden))

//     def iglob(self, pathname, with_matches=False, include_hidden=False):
//         """Return an iterator which yields the paths matching a pathname
//         pattern.
//         The pattern may contain simple shell-style wildcards a la
//         fnmatch. However, unlike fnmatch, filenames starting with a
//         dot are special cases that are not matched by '*' and '?'
//         patterns.
//         If ``with_matches`` is True, then for each matching path
//         a 2-tuple will be returned; the second element if the tuple
//         will be a list of the parts of the path that matched the individual
//         wildcards.
//         If ``include_hidden`` is True, then files and folders starting with
//         a dot are also returned.
//         """
//         result = self._iglob(pathname, include_hidden=include_hidden)
//         if with_matches:
//             return result
//         return map(lambda s: s[0], result)

//     def _iglob(self, pathname, rootcall=True, include_hidden=False):
//         """Internal implementation that backs :meth:`iglob`.
//         ``rootcall`` is required to differentiate between the user's call to
//         iglob(), and subsequent recursive calls, for the purposes of resolving
//         certain special cases of ** wildcards. Specifically, "**" is supposed
//         to include the current directory for purposes of globbing, but the
//         directory itself should never be returned. So if ** is the lastmost
//         part of the ``pathname`` given the user to the root call, we want to
//         ignore the current directory. For this, we need to know which the root
//         call is.
//         """

//         # Short-circuit if no glob magic
//         if not has_magic(pathname):
//             if self.exists(pathname):
//                 yield pathname, ()
//             return

//         # If no directory part is left, assume the working directory
//         dirname, basename = os.path.split(pathname)

//         # If the directory is globbed, recurse to resolve.
//         # If at this point there is no directory part left, we simply
//         # continue with dirname="", which will search the current dir.
//         # `os.path.split()` returns the argument itself as a dirname if it is a
//         # drive or UNC path.  Prevent an infinite recursion if a drive or UNC path
//         # contains magic characters (i.e. r'\\?\C:').
//         if dirname != pathname and has_magic(dirname):
//             # Note that this may return files, which will be ignored
//             # later when we try to use them as directories.
//             # Prefiltering them here would only require more IO ops.
//             dirs = self._iglob(dirname, False, include_hidden)
//         else:
//             dirs = [(dirname, ())]

//         # Resolve ``basename`` expr for every directory found
//         for dirname, dir_groups in dirs:
//             for name, groups in self.resolve_pattern(
//                     dirname, basename, not rootcall, include_hidden):
//                 yield os.path.join(dirname, name), dir_groups + groups

  fun _resolve_pattern(dirname: FilePath, pattern: String, 
      globstar_with_root: Bool, include_hidden: Bool):
        Array[(String, Array[String])] val? =>
    """
    Applies `pattern` (contains no path elements) to the literal directory in
    `dirname`.

    If `pattern==""`, this will filter for directories. This is a special case
    that happens when the user's glob expression ends with a slash (in which
    case we only want directories). It simpler and faster to filter here than
    in `_iglob`.
    """

    // If no magic, short-circuit, only check for existence
    if not has_magic(pattern) then
      if pattern == "" then
        if FileInfo(dirname).directory then
          return recover [(pattern, Array[String])] end
        end
      else
        if FilePath(dirname, pattern).exists() then
          return recover [(pattern, Array[String])] end
        end
      end
      return recover Array[(String, Array[String])] end
    end

    // if not dirname:
    //     dirname = os.curdir

    let names = recover ref Array[String] end
    if pattern == "**" then
      // Include the current directory in **, if asked; by adding
      // an empty string as opposed to '.', we spare ourselves
      // having to deal with os.path.normpath() later.
      if globstar_with_root then
        names.push("")
      end
      dirname.walk(lambda ref(dir_path: FilePath, dir_entries: Array[String] ref)
                         (names: Array[String] ref = names) =>
        for e in dir_entries.values() do
          names.push(Path.join(dir_path.path, e))
        end
      end)
    else
      names.append(Directory(dirname).entries())
    end

    if include_hidden or _is_hidden(pattern) then
      return filter(names, pattern)
    end

    // Remove hidden files, but take care to ensure that the empty string we
    // may have added earlier remains.  Do not filter out the '' that we
    // might have added earlier
    let visible = Array[String](names.size())
    for n in names.values() do
      if not _is_hidden(n) then
        visible.push(n)
      end
    end
    filter(visible, pattern)

