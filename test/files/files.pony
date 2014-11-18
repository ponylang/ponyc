// use "files"
//
// actor Main
//   new create(env: Env) =>
//     try
//       var file = File.open(env.args(1))
//
//       for line in file.lines() do
//         env.stdout.write(line)
//       end
//
//       file.close()
//     end

type Alias is ((Object, Object) | String)

class Object

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    var alias: Alias = "Leaf"

    try_match(alias, 0)

  fun ref try_match(alias: Alias, n: U64) =>
    match (alias, n)
    | (var x: Any, U64(0)) =>
      _env.stdout.print("Expected!")
    | (var x: String, var y: U64) =>
      _env.stdout.print("Not expected: n == " + y.string())
    end
