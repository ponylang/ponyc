// Safe float-to-integer conversions saturate out-of-range values to the
// target type's max or min.  When the integer max is not exactly representable
// in the source float type, the float representation rounds up to the next
// power of two.  The saturation check must catch values at that boundary.
//
// F32(2^31) is above I32.max_value() (2^31 - 1) and must saturate to
// I32.max_value(), not fall through to fptosi (which is undefined behavior
// and produces I32.min_value() on x86-64).  The same applies to
// F64(2^63) -> I64 and F32(2^127) -> I128.

use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    // Multiply by a runtime 1 to prevent constant folding.
    let one = try env.args(1)?.f64()? else F64(1) end

    var ok: Bool = true

    // F32(2^31) -> I32: must saturate to I32.max_value()
    let a = F32(2147483648) * one.f32()
    if a.i32() != I32.max_value() then ok = false end

    // F64(2^63) -> I64: must saturate to I64.max_value()
    let b = F64(9223372036854775808) * one
    if b.i64() != I64.max_value() then ok = false end

    // F32(2^127) -> I128: must saturate to I128.max_value()
    let c = F32(170141183460469231731687303715884105728) * one.f32()
    if c.i128() != I128.max_value() then ok = false end

    // Values below max must pass through unchanged.
    let d = F32(2147483520) * one.f32()
    if d.i32() != I32(2147483520) then ok = false end

    // Values well above max must still saturate.
    let e = F32(4294967296) * one.f32()
    if e.i32() != I32.max_value() then ok = false end

    // Unsigned boundary: F32(2^32) -> U32 must saturate to U32.max_value()
    let f = F32(4294967296) * one.f32()
    if f.u32() != U32.max_value() then ok = false end

    // Unsigned boundary: F64(2^64) -> U64 must saturate to U64.max_value()
    let g = F64(18446744073709551616) * one
    if g.u64() != U64.max_value() then ok = false end

    if ok then @pony_exitcode(1) end
