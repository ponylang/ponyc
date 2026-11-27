// The capture's type is a union with an alias of a union as a member. The
// compiler reaches that type both as written, with the alias, and with the alias
// unfolded into a nested union; both must be the same type. The program must
// compile and print E2.
primitive Continue
primitive NeedMore
primitive E1
primitive E2
type Err is (E1 | E2)

class Parser
  fun deliver(n: USize): (Continue | NeedMore | Err) =>
    if n == 0 then Continue elseif n == 1 then E2 else NeedMore end

  fun parse(n: USize): (Continue | NeedMore | Err) =>
    match \exhaustive\ deliver(n)
    | Continue => None
    | let r: (NeedMore | Err) => return r
    end
    Continue

actor Main
  new create(env: Env) =>
    match Parser.parse(1)
    | E2 => env.out.print("E2")
    else env.out.print("other")
    end
