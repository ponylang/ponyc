// The same lib trait with a type error, used in Main only as a type, with no
// object literal. Main also has its own type error. The test expects both
// errors, unlike the program where Main builds an object literal of the trait.
use l = "lib"
actor Main
  new create(env: Env) =>
    let o: (l.Foo | None) = None
    let x: U8 = "main"
