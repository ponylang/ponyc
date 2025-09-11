primitive Foo
  fun apply(): (ReadSeq[U8] & ReadElement[U8]) =>
    [1; 2; 3]

actor Main
  new create(env: Env) =>
    None
