// C inherits greet() from T, whose body is on lines 11-14 of t.pony, so the
// debug info for C's greet should name t.pony and those lines, not
// main.pony. The test checks only that the program prints 8.
actor Main
  new create(env: Env) =>
    let c = C
    env.out.print(c.greet(3))
class C is T
