use "files"
use "collections"

primitive PrimParam[A]

trait K1
  new tag create()

trait K2
  new iso create()

actor A1 is K1
  new create() => @printf[I32]("create A1 %p\n".cstring(), this)
  be apply() => @printf[I32]("apply A1 %p\n".cstring(), this)

trait T1
  fun tag t1(env: Env) =>
    env.out.print("T1")

trait T2
  fun tag t2(env: Env) =>
    env.out.print("T2")

class Wombat is T1, T2, K1, K2
  new iso create() =>
    @printf[I32]("create Wombat %p\n".cstring(), this)

  fun apply() =>
    @printf[I32]("apply Wombat %p\n".cstring(), this)

actor Wombat2
  be apply(env: Env) =>
    env.out.print("Wombat2")

class Wombat3[A: K2]
  fun apply(): A => A

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    let p1 = PrimParam[K1]
    let p2 = PrimParam[Wombat2]
    let p3 = PrimParam[Wombat2]

    env.out.print((p1 is p2).string())
    env.out.print((p1 is p3).string())
    env.out.print((p2 is p3).string())

    Wombat3[Wombat]()()

    let wombat: (T1 & T2) = Wombat
    wombat.t1(env)
    wombat.t2(env)

    let foo = consume box wombat

    test_primitive()
    test_actor()
    test_literal_ffi()
    test_as_apply()
    test_tuple_map()
    test_assert()
    test_set()

    env.out.write(
      """
      Triple quoted string with empty line:

      Some space should be between this line and the previous one.
      """
      )

  fun ref test_primitive() =>
    let writer = object
      fun tag apply(a: String, b: String): String =>
        a + b
    end

    let partial = writer~apply("foo")
    _env.out.print(partial("bar"))

  fun ref test_actor() =>
    let writer = object
      let _env: Env = _env

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

  fun ref test_as_apply() =>
    let wombat: (Wombat2 | None) = Wombat2
    try (wombat as Wombat2)(_env) end

  fun ref test_tuple_map() =>
    let map = Map[String, (U64, U64)]
    map("hi") = (1, 2)

    try
      let (x, y) = map("hi")
      _env.out.print(x.string() + ", " + y.string())
    end

  fun ref test_assert() =>
    try
      Assert(true, "don't assert")
      Assert(false)
    end

  fun ref test_set() =>
    var set1 = Set[String]
    var set2 = Set[String]
    var set3 = set1.union(set2)
    set1 = set3
