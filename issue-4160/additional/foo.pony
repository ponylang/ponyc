// foo.pony
use "collections"

class Foo
  new create(env: Env) =>
    for i in Range(0, 3) do
        env.out.print(i.string())
    end
    for i in Range(3, 7) do
        env.out.print(i.string())
    end
    for i in Range(7, 10) do
        env.out.print(i.string())
    end
