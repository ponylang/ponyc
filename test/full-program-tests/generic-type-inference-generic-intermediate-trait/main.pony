trait Base[T: Stringable val]
  fun value(): T

trait Middle[T: Stringable val] is Base[T]

class val Impl is Middle[String]
  fun value(): String => "generic"

primitive Runner
  fun run[T: Stringable val](b: Base[T] val): String =>
    b.value().string()

actor Main
  new create(env: Env) =>
    let result = Runner.run(Impl)
    env.out.print(result)
