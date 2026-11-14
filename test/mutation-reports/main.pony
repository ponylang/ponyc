use "collections"
use mutate = "../../tools/pony-mutate"

actor \nodoc\ Main
  new create(env: Env) =>
    if env.args.size() == 1 then return end
    try
      let mode = env.args(1)?
      let first = env.args(2)?
      let second = env.args(3)?
      let text: String val =
        recover val
          let s = String
          for byte in Range[U8].till(0, 32) do s.push(byte) end
          s.append("\"\\ café")
          if mode == "invalid-json" then s.push(0xFF) end
          s
        end
      let outcomes: Array[mutate.MutantOutcome] val =
        if (mode == "empty") or (mode == "uncovered") then
          recover val Array[mutate.MutantOutcome] end
        else
          recover val
            [ mutate.MutantOutcome(
                mutate.Mutant(1, 0, first, 1, 1, text, text, text),
                mutate.MutantKilled,
                0)
              mutate.MutantOutcome(
                _mutant(2, first, 2, 3), mutate.MutantTimedOut, 0)
              mutate.MutantOutcome(
                _mutant(3, second, 1, 2), mutate.MutantTimedOut, 0)
              mutate.MutantOutcome(
                _mutant(4, second, 3, 4), mutate.MutantSurvived, 0)
              mutate.MutantOutcome(
                _mutant(5, first, 2, 1), mutate.MutantSurvived, 0)
              mutate.MutantOutcome(
                _mutant(6, first, 1, 8), mutate.MutantSurvived, 0) ]
          end
        end
      let uncovered: Array[mutate.Mutant] val =
        if mode == "empty" then
          recover val Array[mutate.Mutant] end
        else
          recover val
            [_mutant(7, first, 3, 2); _mutant(8, second, 2, 4)]
          end
        end
      if (mode == "json") or (mode == "invalid-json") then
        mutate.StrykerReporter.report(outcomes, uncovered, env)
      elseif mode == "file" then
        if not mutate.StrykerReporter.write_file(
          outcomes, uncovered, env.args(4)?, env)
        then
          env.exitcode(1)
        end
      else
        mutate.TerminalReporter.report(outcomes, uncovered, env)
      end
    else
      env.err.print("invalid report fixture arguments")
      env.exitcode(1)
    end

  fun _mutant(id: I32, file: String, line: USize, column: USize)
    : mutate.Mutant
  =>
    mutate.Mutant(id, 0, file, line, column, "aor", "+", "-")
