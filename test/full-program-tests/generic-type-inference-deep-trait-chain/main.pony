trait Base[T: Stringable val]
  fun value(): T

trait Mid1 is Base[String]

trait Mid2 is Mid1

class val Impl is Mid2
  fun value(): String => "deep"

primitive Runner
  fun run[T: Stringable val](b: Base[T] val): String =>
    b.value().string()

actor Main
  new create(env: Env) =>
    let result = Runner.run(Impl)
    env.out.print(result)
