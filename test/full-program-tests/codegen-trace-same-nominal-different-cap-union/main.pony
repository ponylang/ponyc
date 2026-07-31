class A

class B
  let a: A = A

actor Main
  new create(env: Env) =>
    let x: (B iso | B val) = recover B end
    receive(consume x)

  be receive(x: (B iso | B val)) => None
