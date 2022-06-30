// main.pony
use "lib:ffi-tuple-parameters-additional"
use @build_vector2_u32[Vector2U32](x: U32, y: U32)
use @build_vector3_u32[Vector3U32](x: U32, y: U32, z: U32)
use @build_vector2_u64[Vector2U64](x: U64, y: U64)
use @build_vector3_u64[Vector3U64](x: U64, y: U64, z: U64)
use @pony_exitcode[None](code: I32)

type Vector2U32 is (U32, U32)
type Vector3U32 is (U32, U32, U32)
type Vector2U64 is (U64, U64)
type Vector3U64 is (U64, U64, U64)

actor Main
  new create(env: Env) =>
    let vector2_u32 = @build_vector2_u32(1, 2)
    env.out.print(vector2_u32._1.string())
    env.out.print(vector2_u32._2.string())

    let vector3_u32 = @build_vector3_u32(3, 4, 5)
    env.out.print(vector3_u32._1.string())
    env.out.print(vector3_u32._2.string())
    env.out.print(vector3_u32._3.string())

    let vector2_u64 = @build_vector2_u64(6, 7)
    env.out.print(vector2_u64._1.string())
    env.out.print(vector2_u64._2.string())

    let vector3_u64 = @build_vector3_u64(8, 9, 10)
    env.out.print(vector3_u64._1.string())
    env.out.print(vector3_u64._2.string())
    env.out.print(vector3_u64._3.string())

    let total_sum: I32 =
      (vector2_u32._1 + vector2_u32._2).i32() +
      (vector3_u32._1 + vector3_u32._2 + vector3_u32._3).i32() +
      (vector2_u64._1 + vector2_u64._2).i32() +
      (vector3_u64._1 + vector3_u64._2 + vector3_u64._3).i32()

    @pony_exitcode(total_sum) // expect 1 + 2 + ... + 10 == 55
