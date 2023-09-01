use "debug"

actor Main
  new create(env: Env) =>
    let a: AA = AA
    inspecta(consume a)

  be inspecta(a: AA val) =>
    Debug.out("in inspecta")

struct iso AA
