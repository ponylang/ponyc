primitive Foo
  fun apply(): (Iterator[U8] | None) =>
    [1; 2; 3].values()

actor Main
  new create(env: Env) =>
    None
