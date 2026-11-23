use @i386_float_bits32[U32]()
use @i386_float_bits64[U64]()
use @i386_call_f32[Bool](receiver: _Float32Callback, fn: Pointer[None])
use @i386_call_f64[Bool](receiver: _ToF64, fn: Pointer[None],
  expected: F64)
use @i386_call_bare[Bool](fn: @{(): F32})

interface val _ToF32
  fun f32(): F32

interface val _ToF64
  fun f64(): F64

class _Float32Callback
  let _value: F32

  new create(value: F32) =>
    _value = value

  fun \noinline\ apply(): F32 =>
    _value

class val _FirstFloat64Callback is _ToF64
  fun \noinline\ f64(): F64 =>
    1.25

class val _SecondFloat64Callback is _ToF64
  fun \noinline\ f64(): F64 =>
    2.5

actor Main
  new create(env: Env) =>
    ifdef x86 and ilp32 then
      try
        let bits32 = @i386_float_bits32()
        let bits64 = @i386_float_bits64()
        let callback32 = _Float32Callback(3.5)
        var failures: I32 = 0
        if _from_bits32(bits32).bits() != bits32 then
          failures = failures or 1
        end
        if _from_bits64(bits64)?.bits() != bits64 then
          failures = failures or 2
        end
        if _boxed32(F32.from_bits(bits32)) != bits32 then
          failures = failures or 4
        end
        if _boxed64(F64.from_bits(bits64)) != bits64 then
          failures = failures or 8
        end
        if not @i386_call_f32(callback32, addressof callback32.apply) then
          failures = failures or 16
        end
        if not (_callback64(_FirstFloat64Callback, 1.25) and
          _callback64(_SecondFloat64Callback, 2.5) and _callback64(F64(4.5), 4.5))
        then
          failures = failures or 32
        end
        try
          _from_bits64(0)?
          failures = failures or 64
        end
        if not _bare32(@{(): F32 => 1.25 }) then
          failures = failures or 128
        end
        env.exitcode(failures)
      else
        env.exitcode(255)
      end
    end

  fun \noinline\ _from_bits32(bits: U32): F32 =>
    F32.from_bits(bits)

  fun \noinline\ _from_bits64(bits: U64): F64 ? =>
    if bits == 0 then error end
    F64.from_bits(bits)

  fun \noinline\ _boxed32(value: _ToF32): U32 =>
    value.f32().bits()

  fun \noinline\ _boxed64(value: _ToF64): U64 =>
    value.f64().bits()

  fun \noinline\ _callback64(receiver: _ToF64, expected: F64): Bool =>
    @i386_call_f64(receiver, addressof receiver.f64, expected)

  fun \noinline\ _bare32(fn: @{(): F32}): Bool =>
    (fn() == 1.25) and @i386_call_bare(addressof fn.apply)
