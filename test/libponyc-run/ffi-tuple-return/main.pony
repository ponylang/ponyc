use "lib:ffi-tuple-return-additional"
use @build_vector2_u32[Vector2U32](x: U32, y: U32)
use @build_vector3_u32[Vector3U32](x: U32, y: U32, z: U32)
use @build_vector2_u64[Vector2U64](x: U64, y: U64)
use @build_vector3_u64[Vector3U64](x: U64, y: U64, z: U64)
use @build_vector2_f32[Vector2F32](x: F32, y: F32)
use @build_vector3_f32[Vector3F32](x: F32, y: F32, z: F32)
use @build_vector4_f32[Vector4F32](x: F32, y: F32, z: F32, u: F32)
use @build_vector5_f32[Vector5F32](x: F32, y: F32, z: F32, u: F32, v: F32)
use @build_vector2_f64[Vector2F64](x: F64, y: F64)
use @build_vector3_f64[Vector3F64](x: F64, y: F64, z: F64)
use @build_vector4_f64[Vector4F64](x: F64, y: F64, z: F64, u: F64)
use @build_vector5_f64[Vector5F64](x: F64, y: F64, z: F64, u: F64, v: F64)
use @build_vector3_mix_a[Vector3MixA](x: U8, y: F32, z: U64)
use @build_vector3_mix_b[Vector3MixB](x: F32, y: F32, z: F64)
use @build_vector3_mix_c[Vector3MixC](x: F64, y: F64, z: F32)
use @pony_exitcode[None](code: I32)

type Vector2U32 is (U32, U32)
type Vector3U32 is (U32, U32, U32)
type Vector2U64 is (U64, U64)
type Vector3U64 is (U64, U64, U64)
type Vector2F32 is (F32, F32)
type Vector3F32 is (F32, F32, F32)
type Vector4F32 is (F32, F32, F32, F32)
type Vector5F32 is (F32, F32, F32, F32, F32)
type Vector2F64 is (F64, F64)
type Vector3F64 is (F64, F64, F64)
type Vector4F64 is (F64, F64, F64, F64)
type Vector5F64 is (F64, F64, F64, F64, F64)
type Vector3MixA is (U8, F32, U64)
type Vector3MixB is (F32, F32, F64)
type Vector3MixC is (F64, F64, F32)

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

    let vector2_f32 = @build_vector2_f32(11, 12)
    env.out.print(vector2_f32._1.string())
    env.out.print(vector2_f32._2.string())

    let vector3_f32 = @build_vector3_f32(13, 14, 15)
    env.out.print(vector3_f32._1.string())
    env.out.print(vector3_f32._2.string())
    env.out.print(vector3_f32._3.string())

    let vector4_f32 = @build_vector4_f32(16, 17, 18, 19)
    env.out.print(vector4_f32._1.string())
    env.out.print(vector4_f32._2.string())
    env.out.print(vector4_f32._3.string())
    env.out.print(vector4_f32._4.string())

    let vector5_f32 = @build_vector5_f32(20, 21, 22, 23, 24)
    env.out.print(vector5_f32._1.string())
    env.out.print(vector5_f32._2.string())
    env.out.print(vector5_f32._3.string())
    env.out.print(vector5_f32._4.string())
    env.out.print(vector5_f32._5.string())

    let vector2_f64 = @build_vector2_f64(25, 26)
    env.out.print(vector2_f64._1.string())
    env.out.print(vector2_f64._2.string())

    let vector3_f64 = @build_vector3_f64(27, 28, 29)
    env.out.print(vector3_f64._1.string())
    env.out.print(vector3_f64._2.string())
    env.out.print(vector3_f64._3.string())

    let vector4_f64 = @build_vector4_f64(30, 31, 32, 33)
    env.out.print(vector4_f64._1.string())
    env.out.print(vector4_f64._2.string())
    env.out.print(vector4_f64._3.string())
    env.out.print(vector4_f64._4.string())

    let vector5_f64 = @build_vector5_f64(34, 35, 36, 37, 38)
    env.out.print(vector5_f64._1.string())
    env.out.print(vector5_f64._2.string())
    env.out.print(vector5_f64._3.string())
    env.out.print(vector5_f64._4.string())
    env.out.print(vector5_f64._5.string())

    let vector3_a = @build_vector3_mix_a(39, 40, 41)
    env.out.print(vector3_a._1.string())
    env.out.print(vector3_a._2.string())
    env.out.print(vector3_a._3.string())

    let vector3_b = @build_vector3_mix_b(42, 43, 44)
    env.out.print(vector3_b._1.string())
    env.out.print(vector3_b._2.string())
    env.out.print(vector3_b._3.string())

    let vector3_c = @build_vector3_mix_c(45, 46, 47)
    env.out.print(vector3_c._1.string())
    env.out.print(vector3_c._2.string())
    env.out.print(vector3_c._3.string())

    let total_sum: I32 =
      (vector2_u32._1 + vector2_u32._2).i32() +
      (vector3_u32._1 + vector3_u32._2 + vector3_u32._3).i32() +
      (vector2_u64._1 + vector2_u64._2).i32() +
      (vector3_u64._1 + vector3_u64._2 + vector3_u64._3).i32() +
      (vector2_f32._1 + vector2_f32._2).i32() +
      (vector3_f32._1 + vector3_f32._2 + vector3_f32._3).i32() +
      (vector4_f32._1 + vector4_f32._2 + vector4_f32._3
        + vector4_f32._4).i32() +
      (vector5_f32._1 + vector5_f32._2 + vector5_f32._3
        + vector5_f32._4 + vector5_f32._5).i32() +
      (vector2_f64._1 + vector2_f64._2).i32() +
      (vector3_f64._1 + vector3_f64._2 + vector3_f64._3).i32() +
      (vector4_f64._1 + vector4_f64._2 + vector4_f64._3
        + vector4_f64._4).i32() +
      (vector5_f64._1 + vector5_f64._2 + vector5_f64._3
        + vector5_f64._4 + vector5_f64._5).i32() +
      (vector3_a._1.i32() + vector3_a._2.i32() + vector3_a._3.i32()) +
      (vector3_b._1.i32() + vector3_b._2.i32() + vector3_b._3.i32()) +
      (vector3_c._1.i32() + vector3_c._2.i32() + vector3_c._3.i32())

    let exit_code = total_sum % 256
    @pony_exitcode(exit_code) // expect (1 + 2 + 3 + ... + 26) % 256 == 104
