// main.pony
use "additional"

actor Main
    new create(env: Env) =>
        let f: Foo = Foo(env)
