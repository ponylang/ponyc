class Greeter
  let greeting: String

  new create(greeting': String = "Hello") =>
    greeting = greeting'

  fun greet(target: String): String =>
    greeting + " " + target

actor Announcer
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  be announce(msg: String) =>
    _out.print(msg)
