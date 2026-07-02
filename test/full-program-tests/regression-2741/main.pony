type Tuple is (
    (U32, String)
  | (U32, U32)
)

actor Main
  new create(env: Env) =>
    this.accept((0x39BF, 0x856D))

  be accept(t: Tuple) =>
    None
