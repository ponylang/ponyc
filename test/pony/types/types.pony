use "files"
use "collections"

trait K1
  new tag create()

trait K2
  new iso create()

actor A1 is K1
  new create() => None

trait T1
  fun tag t1(env: Env) =>
    env.out.print("T1")

trait T2
  fun tag t2(env: Env) =>
    env.out.print("T2")

class Wombat is T1, T2, K1, K2
  new iso create() =>
    None

actor Wombat2
  be apply(env: Env) =>
    env.out.print("Wombat2")

class Wombat3[A: K1 tag]

actor Main
  new create(env: Env) =>
    Wombat3[A1 tag]

    let wombat: (T1 & T2) = Wombat
    wombat.t1(env)
    wombat.t2(env)

    let foo = consume box wombat

    test_primitive(env)
    test_actor(env)
    test_literal_ffi()
    test_as_apply(env)
    test_tuple_map(env)

    env.out.write(
      """
      Triple quoted string with empty line:

      Some space should be between this line and the previous one.
      """
      )

  fun ref test_primitive(env: Env) =>
    let writer = object
      fun tag apply(a: String, b: String): String =>
        a + b
    end

    let partial = writer~apply("foo")
    env.out.print(partial("bar"))

  fun ref test_actor(env: Env) =>
    let writer = object
      let _env: Env = env

      be apply(a: String, b: String) =>
        _env.out.print(a + b)
    end

    let partial = writer~apply("foo")
    partial("bar")

  fun ref test_literal_ffi() =>
    let writer = object
      fun tag apply(a: String, b: String) =>
        @printf[I32]("%s\t%s\n".cstring(), a.cstring(), b.cstring())
    end

    let partial = writer~apply("Test")
    partial("wombat")

  fun ref test_as_apply(env: Env) =>
    let wombat: (Wombat2 | None) = Wombat2
    try (wombat as Wombat2)(env) end

  fun ref test_tuple_map(env: Env) =>
    let map = Map[String, (U64, U64)]
    map("hi") = (1, 2)

    try
      let (x, y) = map("hi")
      env.out.print(x.string() + ", " + y.string())
    end
