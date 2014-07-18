class Env val
  let _args: Array[String]

  new create() => compiler_intrinsic

  fun val args(): Array[String] val => _args
