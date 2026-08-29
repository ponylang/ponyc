use @call_add_f64[F64](x: F64, y: F64)
use @call_add_u32[U32](x: U32, y: U32)
use @call_flag[Bool](x: Bool)
use @call_add_f32[F32](x: F32, y: F32)
use @call_add_isize[ISize](x: ISize, y: ISize)

primitive \c_api\ NumericOps
  fun val add_f64(x: F64, y: F64): F64 => x + y
  fun val add_u32(x: U32, y: U32): U32 => x + y
  fun val flag(x: Bool): Bool => not x
  fun val add_f32(x: F32, y: F32): F32 => x + y
  fun val add_isize(x: ISize, y: ISize): ISize => x + y

actor Main
  new create(env: Env) =>
    let f = @call_add_f64(1.5, 2.5)
    let u = @call_add_u32(20, 22)
    let b = @call_flag(false)
    let f32 = @call_add_f32(F32(1.0), F32(2.0))
    let iz = @call_add_isize(ISize(10), ISize(32))

    if (f == 4.0) and (u == 42) and b
      and (f32 == F32(3.0)) and (iz == 42)
    then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
