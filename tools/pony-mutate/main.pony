use "cli"
use "files"

use @ponyint_cpu_count[U32]()

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf(
          "pony-mutate",
          "Mutation testing runner for Pony programs",
          [
            OptionSpec.u64(
              "workers",
              "Number of parallel worker processes"
              where short' = 'j', default' = 0)
            OptionSpec.u64(
              "timeout",
              "Timeout multiplier relative to baseline (0 = no timeout)"
              where default' = 10)
            OptionSpec.bool(
              "no-baseline",
              "Skip the baseline run"
              where default' = false)
            OptionSpec.string(
              "output",
              "Report format: terminal or json"
              where default' = "terminal")
            OptionSpec.string(
              "json-file",
              "Write Stryker JSON report to this file"
              where default' = "")
            OptionSpec.string(
              "operators",
              "Comma-separated mutation categories to run (aor,bor,ror,sor,uoi)"
              where default' = "")
            OptionSpec.string(
              "scope",
              "Filter mutants to this source file path"
              where default' = "")
          ],
          [
            ArgSpec.string("binary", "Path to the test binary")
          ])? .> add_help()?
      else
        env.err.print("internal error: invalid command spec")
        env.exitcode(1)
        return
      end

    let cmd =
      match \exhaustive\ CommandParser(cs).parse(env.args, env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
        ch.print_help(env.out)
        return
      | let se: SyntaxError =>
        env.err.print(se.string())
        env.exitcode(1)
        return
      end

    let binary = cmd.arg("binary").string()
    if binary.size() == 0 then
      env.err.print("error: binary path is required")
      env.exitcode(1)
      return
    end

    let json_path: String val = binary + ".mutants.json"
    let auth = FileAuth(env.root)

    match \exhaustive\ InfoFile.parse(json_path, auth)
    | (let all_mutants: Array[Mutant] val, InfoFileOk) =>
      let ops_str = cmd.option("operators").string()
      let ops_filter: (String val | None) =
        if ops_str.size() > 0 then ops_str else None end
      let scope_str = cmd.option("scope").string()
      let scope_filter: (String val | None) =
        if scope_str.size() > 0 then scope_str else None end

      let mutants = InfoFile.filter(all_mutants, ops_filter, scope_filter)

      env.err.print(
        "Loaded " + all_mutants.size().string() + " mutants from " +
          json_path)
      if mutants.size() != all_mutants.size() then
        env.err.print(
          "After filtering: " + mutants.size().string() + " mutants")
      end

      let workers_opt = cmd.option("workers").u64().usize()
      let num_workers: USize =
        if workers_opt == 0 then
          @ponyint_cpu_count().usize().max(1)
        else
          workers_opt
        end

      let timeout_mult = cmd.option("timeout").u64()
      let do_baseline = not cmd.option("no-baseline").bool()

      let output_format = cmd.option("output").string()
      let json_file = cmd.option("json-file").string()

      let reporter: Reporter val =
        if output_format == "json" then
          StrykerReporter
        else
          TerminalReporter
        end

      let binary_path = FilePath(auth, binary)
      let args: Array[String] val =
        recover val
          Array[String]
            .> push(binary)
            .> push("--ponynoscale")
            .> push("--ponymaxthreads=1")
        end
      let base_vars: Array[String] val = env.vars

      let json_out: (String val | None) =
        if json_file.size() > 0 then json_file else None end

      Runner(
        env,
        binary_path,
        args,
        base_vars,
        mutants,
        num_workers,
        timeout_mult,
        reporter,
        json_out,
        do_baseline)
    | (let err: String val, InfoFileError) =>
      env.err.print("error: " + err)
      env.exitcode(1)
    end
