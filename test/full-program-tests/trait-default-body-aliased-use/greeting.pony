use collections = "collections"

trait Greeting
  fun hello(env: Env) =>
    let hi = collections.Map[String, String]
    hi.insert("hello", "world!")
    try env.out.print(hi("hello")?) end
