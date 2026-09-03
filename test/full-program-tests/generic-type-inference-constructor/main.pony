class Wrapper[A: Any val]
  let _a: A
  new create(a: A) => _a = a
  fun get(): A => _a

actor Main
  new create(env: Env) =>
    let w = Wrapper("hello")
    let s: String val = w.get()

    let w2 = Wrapper.create(U8(42))
    let n: U8 = w2.get()
