use "term"

class Handler is ReadlineNotify
  let _commands: Array[String] = _commands.create()

  new create() =>
    _commands.push("quit")
    _commands.push("happy")
    _commands.push("hello")

  fun ref apply(line: String): String ? =>
    if line == "quit" then
      error
    end

    "> "

  fun ref tab(line: String): Seq[String] box =>
    let r = Array[String]

    for command in _commands.values() do
      if command.at(line, 0) then
        r.push(command)
      end
    end

    r

actor Main
  new create(env: Env) =>
    env.out.print("Use 'quit' to exit.")
    env.input(ANSITerm(Readline(recover Handler end, env.out)))
