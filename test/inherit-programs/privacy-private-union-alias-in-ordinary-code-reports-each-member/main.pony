// main calls name() through its alias of a union of lib's private _H and _H2.
// Expects a private lookup error and a couldn't-find error for each member,
// four in all: the errors inherited code reports for the same call.
use l = "lib"
actor Main
  new create(env: Env) =>
    let x: l.PubH = l.MakeH()
    env.out.print(x.name())
