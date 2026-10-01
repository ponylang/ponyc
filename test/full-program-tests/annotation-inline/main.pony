primitive Foo
  fun \inline\ always(): U64 => 42
  fun \inline(500)\ threshold(): U64 => 43
  fun \noinline\ never(): U64 => 44

actor Main
  new create(env: Env) =>
    env.out.print(
      Foo.always().string() + " " +
      Foo.threshold().string() + " " +
      Foo.never().string())
