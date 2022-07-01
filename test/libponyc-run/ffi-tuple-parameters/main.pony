use "lib:ffi-tuple-parameters-additional"
use @sum_vector2_u32[I32](vec: Vector2U32)
use @sum_vector3_u32[I32](vec: Vector3U32)
use @sum_vector2_u64[I32](vec: Vector2U64)
use @sum_vector3_u64[I32](vec: Vector3U64)
use @sum_vector2_f32[I32](vec: Vector2F32)
use @sum_vector3_f32[I32](vec: Vector3F32)
use @sum_vector4_f32[I32](vec: Vector4F32)
use @sum_vector5_f32[I32](vec: Vector5F32)
use @sum_vector2_f64[I32](vec: Vector2F64)
use @sum_vector3_f64[I32](vec: Vector3F64)
use @sum_vector4_f64[I32](vec: Vector4F64)
use @sum_vector5_f64[I32](vec: Vector5F64)
use @sum_vector3_mix_a[I32](vec: Vector3MixA)
use @sum_vector3_mix_b[I32](vec: Vector3MixB)
use @sum_vector3_mix_c[I32](vec: Vector3MixC)
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
    let vector2_u32_sum = @sum_vector2_u32((1, 2))
    env.out.print(vector2_u32_sum.string() + " (expected 3)")

    let vector3_u32_sum = @sum_vector3_u32((3, 4, 5))
    env.out.print(vector3_u32_sum.string() + " (expected 12)")

    let vector2_u64_sum = @sum_vector2_u64((6, 7))
    env.out.print(vector2_u64_sum.string() + " (expected 13)")

    let vector3_u64_sum = @sum_vector3_u64((8, 9, 10))
    env.out.print(vector3_u64_sum.string() + " (expected 27)")

    let vector2_f32_sum = @sum_vector2_f32((11, 12))
    env.out.print(vector2_f32_sum.string() + " (expected 23)")

    let vector3_f32_sum = @sum_vector3_f32((13, 14, 15))
    env.out.print(vector3_f32_sum.string() + " (expected 42)")

    let vector4_f32_sum = @sum_vector4_f32((16, 17, 18, 19))
    env.out.print(vector4_f32_sum.string() + " (expected 70)")

    let vector5_f32_sum = @sum_vector5_f32((20, 21, 22, 23, 24))
    env.out.print(vector5_f32_sum.string() + " (expected 110)")

    let vector2_f64_sum = @sum_vector2_f64((25, 26))
    env.out.print(vector2_f64_sum.string() + " (expected 51)")

    let vector3_f64_sum = @sum_vector3_f64((27, 28, 29))
    env.out.print(vector3_f64_sum.string() + " (expected 84)")

    let vector4_f64_sum = @sum_vector4_f64((30, 31, 32, 33))
    env.out.print(vector4_f64_sum.string() + " (expected 126)")

    let vector5_f64_sum = @sum_vector5_f64((34, 35, 36, 37, 38))
    env.out.print(vector5_f64_sum.string() + " (expected 185)")

    let vector3_mix_a_sum = @sum_vector3_mix_a((39, 40, 41))
    env.out.print(vector3_mix_a_sum.string() + " (expected 120)")

    let vector3_mix_b_sum = @sum_vector3_mix_b((42, 43, 44))
    env.out.print(vector3_mix_b_sum.string() + " (expected 129)")

    let vector3_mix_c_sum = @sum_vector3_mix_c((45, 46, 47))
    env.out.print(vector3_mix_c_sum.string() + " (expected 138)")

    let total_sum: I32 =
      vector2_u32_sum +
      vector3_u32_sum +
      vector2_u64_sum +
      vector3_u64_sum +
      vector2_f32_sum +
      vector3_f32_sum +
      vector4_f32_sum +
      vector5_f32_sum +
      vector2_f64_sum +
      vector3_f64_sum +
      vector4_f64_sum +
      vector5_f64_sum +
      vector3_mix_a_sum +
      vector3_mix_b_sum +
      vector3_mix_c_sum

    let exit_code = total_sum % 256
    @pony_exitcode(exit_code) // expect (1 + 2 + 3 + ... + 47) % 256 == 104
