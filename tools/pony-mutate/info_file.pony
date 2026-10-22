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
        for i in Range(0, arr.size()) do
          let m = arr(i)? as JSONObject
          let id = m("id")? as I64
          let file = m("file")? as String
          let line = m("line")? as I64
          let column = m("column")? as I64
          let category = m("category")? as String
          let original = m("original")? as String
          let replacement = m("replacement")? as String
          mutants.push(
            Mutant(
              id.i32(),
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
    Check whether `file` ends with `scope` on a path boundary.
    """
    if scope.size() > file.size() then return false end
    let offset = file.size() - scope.size()
    if not file.at(scope, ISize.from[USize](offset)) then
      return false
    end
    if offset == 0 then return true end
    try
      let sep = file(offset - 1)?
      (sep == '/') or (sep == '\\')
    else
      false
    end

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
