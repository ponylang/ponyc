use "collections"

interface val Reporter
  """
  Formats and outputs mutation testing results.
  """
  fun report(outcomes: Array[MutantOutcome] val, env: Env)
    """
    Write the mutation testing report to the environment's output.
    """

primitive TerminalReporter is Reporter
  """
  Prints a human-readable mutation testing summary to stdout.
  """

  fun report(outcomes: Array[MutantOutcome] val, env: Env) =>
    """
    Print the terminal report.
    """
    var killed: USize = 0
    var survived: USize = 0
    var timed_out: USize = 0
    let survived_list = Array[MutantOutcome]

    for o in outcomes.values() do
      match \exhaustive\ o.result
      | MutantKilled => killed = killed + 1
      | MutantSurvived =>
        survived = survived + 1
        survived_list.push(o)
      | MutantTimedOut => timed_out = timed_out + 1
      end
    end

    let total = outcomes.size()
    let score =
      if total > 0 then
        (((killed + timed_out).f64() / total.f64()) * 1000.0).u64()
      else
        U64(0)
      end
    let score_whole = score / 10
    let score_frac = score % 10

    env.out.print("")
    env.out.print("Mutation Testing Report")
    env.out.print("=======================")
    env.out.print("")
    env.out.print("Total mutants:  " + total.string())
    env.out.print(
      "Killed:         " + killed.string() +
        " (" + _pct(killed, total) + ")")
    env.out.print(
      "Survived:       " + survived.string() +
        " (" + _pct(survived, total) + ")")
    env.out.print(
      "Timed out:      " + timed_out.string() +
        " (" + _pct(timed_out, total) + ")")
    env.out.print("")
    env.out.print(
      "Mutation score: " + score_whole.string() + "." +
        score_frac.string() +
        "% (killed + timed out) / total")

    if survived_list.size() > 0 then
      env.out.print("")
      env.out.print("Survived mutants:")
      Sort[Array[MutantOutcome], MutantOutcome](survived_list)
      for o in survived_list.values() do
        env.out.print(
          "  " + o.mutant.location() +
            "  " + o.mutant.category +
            "  " + o.mutant.original +
            " -> " + o.mutant.replacement)
      end
    end

  fun _pct(n: USize, total: USize): String val =>
    if total == 0 then
      "0.0%"
    else
      let p = ((n.f64() / total.f64()) * 1000.0).u64()
      (p / 10).string() + "." + (p % 10).string() + "%"
    end
