trait Base[T: Stringable val]
  fun value(): T

trait Middle is Base[String]

class val Impl is Middle
  fun value(): String => "inferred"

primitive Runner
  fun run[T: Stringable val](b: Base[T] val): String =>
    b.value().string()

actor Main
  new create(env: Env) =>
    let result = Runner.run(Impl)
    env.out.print(result)
