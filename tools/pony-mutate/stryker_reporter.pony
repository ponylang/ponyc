use "collections"
use "files"

primitive StrykerReporter is Reporter
  """
  Writes a Stryker mutation testing report (JSON) conforming to the
  mutation-testing-elements schema.
  """
  fun report(
    outcomes: Array[MutantOutcome] val,
    no_coverage: Array[Mutant] val,
    env: Env)
  =>
    """
    Print the Stryker JSON report to stdout. Source files must be readable
    UTF-8 text.
    """
    try
      env.out.print(_build(outcomes, no_coverage, env)?)
    else
      env.exitcode(1)
    end

  fun write_file(
    outcomes: Array[MutantOutcome] val,
    no_coverage: Array[Mutant] val,
    path: String val,
    env: Env)
    : Bool
  =>
    """
    Replace `path` with the Stryker JSON report. Returns true on success.
    Source files must be readable UTF-8 text.
    """
    let data =
      try _build(outcomes, no_coverage, env)?
      else return false
      end
    let fp = FilePath(FileAuth(env.root), path)
    match CreateFile(fp)
    | let file: File =>
      let written = file.set_length(0) and file.print(consume data)
      file.dispose()
      if written then return true end
    end
    env.err.print("error: cannot write " + path)
    false

  fun _build(
    outcomes: Array[MutantOutcome] val,
    no_coverage: Array[Mutant] val,
    env: Env)
    : String iso^ ?
  =>
    let by_file = Map[String val, Array[_StrykerEntry]]
    for o in outcomes.values() do
      let arr =
        by_file.get_or_else(
          o.mutant.file,
          Array[_StrykerEntry])
      arr.push(_StrykerEntry(o.mutant, _status(o.result)))
      by_file(o.mutant.file) = arr
    end
    for m in no_coverage.values() do
      let arr =
        by_file.get_or_else(
          m.file,
          Array[_StrykerEntry])
      arr.push(_StrykerEntry(m, "NoCoverage"))
      by_file(m.file) = arr
    end

    let buf = recover iso String end
    buf.append("{\n")
    buf.append("  \"schemaVersion\": \"1\",\n")
    buf.append("  \"thresholds\": {\"high\": 80, \"low\": 60},\n")
    buf.append("  \"files\": {\n")

    var file_idx: USize = 0
    for (file, entries) in by_file.pairs() do
      if file_idx > 0 then buf.append(",\n") end
      buf.append("    ")
      buf.append(_escape(file, env, file)?)
      buf.append(": {\n")
      buf.append("      \"language\": \"pony\",\n")
      buf.append("      \"source\": ")
      buf.append(_escape(_source(file, env)?, env, file)?)
      buf.append(",\n")
      buf.append("      \"mutants\": [\n")

      for (i, e) in entries.pairs() do
        if i > 0 then buf.append(",\n") end
        buf.append("        {\n")
        buf.append("          \"id\": \"")
        buf.append(e.mutant.id.string())
        buf.append("\",\n")
        buf.append("          \"mutatorName\": ")
        buf.append(_escape(
          e.mutant.category + "/" + e.mutant.original +
            "->" + e.mutant.replacement,
          env,
          file)?)
        buf.append(",\n")
        buf.append("          \"replacement\": ")
        buf.append(_escape(e.mutant.replacement, env, file)?)
        buf.append(",\n")
        buf.append("          \"location\": {\n")
        buf.append("            \"start\": {\"line\": ")
        buf.append(e.mutant.line.string())
        buf.append(", \"column\": ")
        buf.append(e.mutant.column.string())
        buf.append("},\n")
        buf.append("            \"end\": {\"line\": ")
        buf.append(e.mutant.line.string())
        buf.append(", \"column\": ")
        buf.append((e.mutant.column + 1).string())
        buf.append("}\n")
        buf.append("          },\n")
        buf.append("          \"status\": ")
        buf.append(_escape(e.status, env, file)?)
        buf.append("\n")
        buf.append("        }")
      end

      buf.append("\n      ]\n")
      buf.append("    }")
      file_idx = file_idx + 1
    end

    buf.append("\n  }\n")
    buf.append("}\n")
    consume buf

  fun _source(path: String val, env: Env): String val ? =>
    match OpenFile(FilePath(FileAuth(env.root), path))
    | let file: File =>
      let size = file.size()
      let data: String val = file.read_string(size)
      let errno = file.errno()
      let read_ok =
        (data.size() == size) and
        ((errno is FileOK) or ((size == 0) and (errno is FileEOF)))
      file.dispose()
      if read_ok then return data end
    end
    env.err.print("error: cannot read source " + path)
    error

  fun _status(r: MutantResult): String val =>
    match \exhaustive\ r
    | MutantKilled => "Killed"
    | MutantSurvived => "Survived"
    | MutantTimedOut => "Timeout"
    end

  fun _escape(s: String val, env: Env, path: String val): String val ? =>
    let out = recover iso String(s.size() + 2) end
    out.push('"')
    var offset: USize = 0
    while offset < s.size() do
      (let rune, let len) =
        try s.utf32(offset.isize())?
        else
          _Unreachable()
          (U32(0), U8(1))
        end
      if (rune == 0xFFFD) and (len == 1) then
        env.err.print("error: invalid UTF-8 in JSON report for " + path)
        error
      end
      match rune
      | '"' => out.append("\\\"")
      | '\\' => out.append("\\\\")
      | '\n' => out.append("\\n")
      | '\t' => out.append("\\t")
      | '\r' => out.append("\\r")
      else
        if rune < 0x20 then
          let byte = rune.u8()
          out.append("\\u00")
          let hi = (byte >> 4) and 0x0F
          let lo = byte and 0x0F
          out.push(if hi < 10 then '0' + hi else ('a' - 10) + hi end)
          out.push(if lo < 10 then '0' + lo else ('a' - 10) + lo end)
        else
          out.push_utf32(rune)
        end
      end
      offset = offset + len.usize()
    end
    out.push('"')
    consume out

class val _StrykerEntry
  let mutant: Mutant
  let status: String val

  new val create(mutant': Mutant, status': String val) =>
    mutant = mutant'
    status = status'
