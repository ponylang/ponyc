use "files"

trait T1
  fun tag t1(env: Env) =>
    env.out.print("T1")

trait T2
  fun tag t2(env: Env) =>
    env.out.print("T2")

class Wombat is T1, T2

actor Main
  new create(env: Env) =>
    let wombat: (T1 & T2) = Wombat
    wombat.t1(env)
    wombat.t2(env)

    test_primitive(env)
    test_actor(env)

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
