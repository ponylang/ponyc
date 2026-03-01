actor Main
  new create(env: Env) =>
    Dealer.from[U8](0)

class Dealer
  new from[U: (U8 | Dealer val)](u: U val) =>
    match \exhaustive\ u
    | let u': U8 => None
    | let u': Dealer val => None
    end
