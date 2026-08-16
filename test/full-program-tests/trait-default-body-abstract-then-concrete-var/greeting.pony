trait Greeting
  fun hello(env: Env) =>
    var msg: String = "hello"
    msg = msg + " world"
    env.out.print(msg)
