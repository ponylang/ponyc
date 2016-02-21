use "term"
use "promises"

class Handler is ReadlineNotify
  let _commands: Array[String] = _commands.create()
  var _i: U64 = 0

  new create() =>
    _commands.push("quit")
    _commands.push("happy")
    _commands.push("hello")

  fun ref apply(line: String, prompt: Promise[String]) =>
    if line == "quit" then
      prompt.reject()
    else
      _i = _i + 1
      prompt(_i.string() + " > ")
    end

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

    // Building a delegate manually
    let term = ANSITerm(Readline(recover Handler end, env.out), env.input)
    term.prompt("0 > ")

    let notify = object iso
      let term: ANSITerm = term
      fun ref apply(data: Array[U8] iso) => term(consume data)
      fun ref dispose() => term.dispose()
    end

    env.input(consume notify)
