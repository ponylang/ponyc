use "collections"
use "files"

primitive StrykerReporter is Reporter
  """
  Writes a Stryker mutation testing report (JSON) conforming to the
  mutation-testing-elements schema.
  """
  fun report(outcomes: Array[MutantOutcome] val, env: Env) =>
    """
    Print the Stryker JSON report to stdout.
    """
    env.out.print(_build(outcomes))
    for o in outcomes.values() do
      if o.result is MutantSurvived then
        env.exitcode(1)
        return
      end
    end

  fun write_file(
    outcomes: Array[MutantOutcome] val,
    path: String val,
    env: Env)
  =>
    let fp = FilePath(FileAuth(env.root), path)
    match CreateFile(fp)
    | let file: File =>
      file.print(_build(outcomes))
    else
      env.err.print("error: cannot write " + path)
      env.exitcode(1)
    end

  fun _build(outcomes: Array[MutantOutcome] val): String iso^ =>
    let by_file = Map[String val, Array[MutantOutcome]]
    for o in outcomes.values() do
      let arr =
        by_file.get_or_else(
          o.mutant.file,
          Array[MutantOutcome])
      arr.push(o)
      by_file(o.mutant.file) = arr
    end

    let buf = recover iso String end
    buf.append("{\n")
    buf.append("  \"schemaVersion\": \"1\",\n")
    buf.append("  \"thresholds\": {\"high\": 80, \"low\": 60},\n")
    buf.append("  \"files\": {\n")

    var file_idx: USize = 0
    for (file, file_outcomes) in by_file.pairs() do
      if file_idx > 0 then buf.append(",\n") end
      buf.append("    ")
      buf.append(_escape(file))
      buf.append(": {\n")
      buf.append("      \"language\": \"pony\",\n")
      buf.append("      \"mutants\": [\n")

      for (i, o) in file_outcomes.pairs() do
        if i > 0 then buf.append(",\n") end
        buf.append("        {\n")
        buf.append("          \"id\": \"")
        buf.append(o.mutant.id.string())
        buf.append("\",\n")
        buf.append("          \"mutatorName\": ")
        buf.append(_escape(
          o.mutant.category + "/" + o.mutant.original +
            "->" + o.mutant.replacement))
        buf.append(",\n")
        buf.append("          \"replacement\": ")
        buf.append(_escape(o.mutant.replacement))
        buf.append(",\n")
        buf.append("          \"location\": {\n")
        buf.append("            \"start\": {\"line\": ")
        buf.append(o.mutant.line.string())
        buf.append(", \"column\": ")
        buf.append(o.mutant.column.string())
        buf.append("},\n")
        buf.append("            \"end\": {\"line\": ")
        buf.append(o.mutant.line.string())
        buf.append(", \"column\": ")
        buf.append((o.mutant.column + 1).string())
        buf.append("}\n")
        buf.append("          },\n")
        buf.append("          \"status\": ")
        buf.append(_escape(_status(o.result)))
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

  fun _status(r: MutantResult): String val =>
    match \exhaustive\ r
    | MutantKilled => "Killed"
    | MutantSurvived => "Survived"
    | MutantTimedOut => "Timeout"
    end

  fun _escape(s: String val): String val =>
    recover val
      let out = String(s.size() + 2)
      out.push('"')
      for byte in s.values() do
        match byte
        | '"' => out.append("\\\"")
        | '\\' => out.append("\\\\")
        | '\n' => out.append("\\n")
        | '\t' => out.append("\\t")
        | '\r' => out.append("\\r")
        else
          if byte < 0x20 then
            out.append("\\u00")
            let hi = (byte >> 4) and 0x0F
            let lo = byte and 0x0F
            out.push(if hi < 10 then '0' + hi else ('a' - 10) + hi end)
            out.push(if lo < 10 then '0' + lo else ('a' - 10) + lo end)
          else
            out.push(byte)
          end
        end
      end
      out.push('"')
      out
    end
