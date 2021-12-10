actor Main
  new create(env: Env) =>
    apply(env)

  fun apply(env: Env) =>
    test[I64](env, (0, true))
    test[U64](env, (0, true))

  fun test[A: Stringable #read](env: Env, expected: (A, Bool)) =>
    busywork[A](env, expected._1)

  fun busywork[A: Stringable #read](env: Env, expected: A) =>
    "." + "." + "." + "." + "." + "." + "." + "." + "." + "." + "." + "." + "."
