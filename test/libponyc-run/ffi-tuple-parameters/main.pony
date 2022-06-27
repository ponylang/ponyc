// main.pony
use "lib:ffi-tuple-parameters-additional"
use @build_vector[Vector](ini: U64)
use @pony_exitcode[None](code: I32)

type Vector is (U64, U64, U64)

actor Main
  new create(env: Env) =>
    let vector = @build_vector(1)
    @pony_exitcode((vector._1 + vector._2 + vector._3).i32()) // expect 3
