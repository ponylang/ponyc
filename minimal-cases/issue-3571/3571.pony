class A
class BadExtract
  var a: A trn = A
  var a_alias: A box = a

  new trn create() => None

  fun ref extract_trn(): A trn^ =>
    a = A

actor Main
  new create(env: Env) =>
    let bad = BadExtract
    let a_ref: A ref = bad.extract_trn()
    let a_val: A val = (consume val bad).a_alias

    env.out.print((a_ref is a_val).string())

