use "collections"
use "files"
use "json"

primitive InfoFileError
  """
  A parse or I/O failure when loading the info file.
  """

primitive InfoFileOk
  """
  The info file loaded successfully.
  """

type InfoFileResult is
  ( (Array[Mutant] val, InfoFileOk)
  | (String val, InfoFileError) )

primitive InfoFile
  """
  Parses a `.mutants.json` info file into a list of mutants.
  """

  fun parse(path: String val, auth: FileAuth): InfoFileResult =>
    """
    Load and decode a `.mutants.json` file, returning the mutant list or an
    error string.
    """
    let fp = FilePath(auth, path)
    let data =
      match OpenFile(fp)
      | let file: File =>
        file.read_string(file.size())
      else
        return ("cannot open " + path, InfoFileError)
      end

    match JSONParser.parse(consume data)
    | let obj: JSONObject =>
      try
        let arr = obj("mutants")? as JSONArray
        let mutants = recover iso Array[Mutant] end
        for i in Range[USize].till(0, arr.size()) do
          let m = arr(i)? as JSONObject
          let id = m("id")? as I64
          let file = m("file")? as String
          let line = m("line")? as I64
          let column = m("column")? as I64
          let category = m("category")? as String
          let original = m("original")? as String
          let replacement = m("replacement")? as String
          let cov_counter: I64 =
            try m("coverage_counter")? as I64 else 0 end
          mutants.push(
            Mutant(
              id.i32(),
              cov_counter.i32(),
              file,
              line.usize(),
              column.usize(),
              category,
              original,
              replacement))
        end
        (consume mutants, InfoFileOk)
      else
        ("malformed mutant record in " + path, InfoFileError)
      end
    | let err: JSONParseError =>
      (path + ": " + err.string(), InfoFileError)
    else
      (path + ": top-level value is not an object", InfoFileError)
    end

  fun filter(
    mutants: Array[Mutant] val,
    operators: (String val | None) = None,
    scope: (String val | None) = None)
    : Array[Mutant] val
  =>
    """
    Filter mutants by operator category and/or source file scope.
    """
    let op_set = _parse_operators(operators)
    let scope_file = _parse_scope(scope)

    if (op_set.size() == 0) and (scope_file is None) then
      return mutants
    end

    let result = recover iso Array[Mutant] end
    for m in mutants.values() do
      if (op_set.size() > 0) and not op_set.contains(m.category) then
        continue
      end
      match scope_file
      | let sf: String val =>
        if not _file_matches(m.file, sf) then
          continue
        end
      end
      result.push(m)
    end
    consume result

  fun _file_matches(file: String val, scope: String val): Bool =>
    """
    Check whether `file` contains `scope` as a path component or sequence
    of path components (bounded by `/`, `\`, or string edges).
    """
    if (scope.size() == 0) or (scope.size() > file.size()) then
      return false
    end
    var search_from: ISize = 0
    while search_from.usize() < file.size() do
      let pos =
        try file.find(scope where offset = search_from)?
        else return false
        end
      let p = pos.usize()
      let left_ok =
        if p == 0 then true
        else
          try
            let c = file(p - 1)?
            (c == '/') or (c == '\\')
          else
            false
          end
        end
      let end_pos = p + scope.size()
      let right_ok =
        if end_pos >= file.size() then true
        else
          try
            let c = file(end_pos)?
            (c == '/') or (c == '\\')
          else
            false
          end
        end
      if left_ok and right_ok then return true end
      search_from = pos + 1
    end
    false

  fun _parse_operators(ops: (String val | None)): Set[String val] =>
    match ops
    | let s: String val =>
      let set = Set[String val]
      for part in s.split(",").values() do
        let trimmed = part.clone() .> strip()
        set.set(consume trimmed)
      end
      set
    else
      Set[String val]
    end

  fun _parse_scope(scope: (String val | None)): (String val | None) =>
    match scope
    | let s: String val if s.size() > 0 => s
    else
      None
    end
