use "pony_bench"

class iso _BoxU32 is MicroBenchmark
  var _n: U32 = 0

  fun name(): String => "BoxU32"

  fun ref apply() =>
    _n = _n + 1
    let x: (U32 | None) = _n
    DoNotOptimise[(U32 | None)](x)
    DoNotOptimise.observe()

class iso _BoxU8 is MicroBenchmark
  var _n: U8 = 0

  fun name(): String => "BoxU8"

  fun ref apply() =>
    _n = _n + 1
    let x: (U8 | None) = _n
    DoNotOptimise[(U8 | None)](x)
    DoNotOptimise.observe()

class iso _BoxBool is MicroBenchmark
  var _flip: Bool = true

  fun name(): String => "BoxBool"

  fun ref apply() =>
    _flip = not _flip
    let x: (Bool | None) = _flip
    DoNotOptimise[(Bool | None)](x)
    DoNotOptimise.observe()

class iso _BoxF32 is MicroBenchmark
  var _n: F32 = 0.0

  fun name(): String => "BoxF32"

  fun ref apply() =>
    _n = _n + 1.0
    let x: (F32 | None) = _n
    DoNotOptimise[(F32 | None)](x)
    DoNotOptimise.observe()

class iso _UnboxU32 is MicroBenchmark
  var _n: U32 = 0

  fun name(): String => "UnboxU32"

  fun ref apply() =>
    _n = _n + 1
    let boxed: (U32 | None) = _n
    DoNotOptimise[(U32 | None)](boxed)
    DoNotOptimise.observe()
    match boxed
    | let v: U32 =>
      DoNotOptimise[U32](v)
      DoNotOptimise.observe()
    end

class iso _UnboxF32 is MicroBenchmark
  var _n: F32 = 0.0

  fun name(): String => "UnboxF32"

  fun ref apply() =>
    _n = _n + 1.0
    let boxed: (F32 | None) = _n
    DoNotOptimise[(F32 | None)](boxed)
    DoNotOptimise.observe()
    match boxed
    | let v: F32 =>
      DoNotOptimise[F32](v)
      DoNotOptimise.observe()
    end

class iso _MatchU32OrNone is MicroBenchmark
  var _n: U32 = 0

  fun name(): String => "MatchU32OrNone"

  fun ref apply() =>
    _n = _n + 1
    let x: (U32 | None) = if (_n % 2) == 0 then _n else None end
    DoNotOptimise[(U32 | None)](x)
    DoNotOptimise.observe()
    match x
    | let v: U32 =>
      DoNotOptimise[U32](v)
      DoNotOptimise.observe()
    | None =>
      DoNotOptimise[None](None)
      DoNotOptimise.observe()
    end

class iso _MatchThreeWay is MicroBenchmark
  var _n: U32 = 0

  fun name(): String => "MatchThreeWay"

  fun ref apply() =>
    _n = _n + 1
    let x: (U32 | F32 | Bool) =
      match _n % 3
      | 0 => _n
      | 1 => _n.f32()
      else true
      end
    DoNotOptimise[(U32 | F32 | Bool)](x)
    DoNotOptimise.observe()
    match x
    | let v: U32 =>
      DoNotOptimise[U32](v)
      DoNotOptimise.observe()
    | let v: F32 =>
      DoNotOptimise[F32](v)
      DoNotOptimise.observe()
    | let v: Bool =>
      DoNotOptimise[Bool](v)
      DoNotOptimise.observe()
    end

class iso _IsBoxedU32 is MicroBenchmark
  var _n: U32 = 0

  fun name(): String => "IsBoxedU32"

  fun ref apply() =>
    _n = _n + 1
    let a: (U32 | None) = _n
    let b: (U32 | None) = _n
    DoNotOptimise[(U32 | None)](a)
    DoNotOptimise[(U32 | None)](b)
    DoNotOptimise.observe()
    let eq = a is b
    DoNotOptimise[Bool](eq)
    DoNotOptimise.observe()

class iso _IsBoxedNone is MicroBenchmark
  fun name(): String => "IsBoxedNone"

  fun ref apply() =>
    let a: (U32 | None) = None
    let b: (U32 | None) = None
    DoNotOptimise[(U32 | None)](a)
    DoNotOptimise[(U32 | None)](b)
    DoNotOptimise.observe()
    let eq = a is b
    DoNotOptimise[Bool](eq)
    DoNotOptimise.observe()

class iso _IsntBoxedU32 is MicroBenchmark
  var _n: U32 = 0

  fun name(): String => "IsntBoxedU32"

  fun ref apply() =>
    _n = _n + 1
    let a: (U32 | None) = _n
    let b: (U32 | None) = _n + 1
    DoNotOptimise[(U32 | None)](a)
    DoNotOptimise[(U32 | None)](b)
    DoNotOptimise.observe()
    let eq = a is b
    DoNotOptimise[Bool](eq)
    DoNotOptimise.observe()

class iso _ArrayStoreBoxed is MicroBenchmark
  let _arr: Array[(U32 | None)]

  new iso create() =>
    _arr = Array[(U32 | None)](64)
    var i: USize = 0
    while i < 64 do
      _arr.push(None)
      i = i + 1
    end

  fun name(): String => "ArrayStoreBoxed"

  fun ref apply() =>
    var i: USize = 0
    while i < 64 do
      try _arr(i)? = U32(i.u32()) end
      i = i + 1
    end

class iso _ArrayLoadBoxed is MicroBenchmark
  let _arr: Array[(U32 | None)]

  new iso create() =>
    _arr = Array[(U32 | None)](64)
    var i: USize = 0
    while i < 64 do
      _arr.push(U32(i.u32()))
      i = i + 1
    end

  fun name(): String => "ArrayLoadBoxed"

  fun ref apply() =>
    var sum: U32 = 0
    var i: USize = 0
    while i < 64 do
      match try _arr(i)? end
      | let v: U32 => sum = sum + v
      end
      i = i + 1
    end
    DoNotOptimise[U32](sum)
    DoNotOptimise.observe()

// Non-taggable types (still heap-boxed) — regression check
class iso _BoxU64 is MicroBenchmark
  var _n: U64 = 0

  fun name(): String => "BoxU64"

  fun ref apply() =>
    _n = _n + 1
    let x: (U64 | None) = _n
    DoNotOptimise[(U64 | None)](x)
    DoNotOptimise.observe()

class iso _UnboxU64 is MicroBenchmark
  var _n: U64 = 0

  fun name(): String => "UnboxU64"

  fun ref apply() =>
    _n = _n + 1
    let boxed: (U64 | None) = _n
    DoNotOptimise[(U64 | None)](boxed)
    DoNotOptimise.observe()
    match boxed
    | let v: U64 =>
      DoNotOptimise[U64](v)
      DoNotOptimise.observe()
    end

class iso _BoxF64 is MicroBenchmark
  var _n: F64 = 0.0

  fun name(): String => "BoxF64"

  fun ref apply() =>
    _n = _n + 1.0
    let x: (F64 | None) = _n
    DoNotOptimise[(F64 | None)](x)
    DoNotOptimise.observe()

class iso _UnboxF64 is MicroBenchmark
  var _n: F64 = 0.0

  fun name(): String => "UnboxF64"

  fun ref apply() =>
    _n = _n + 1.0
    let boxed: (F64 | None) = _n
    DoNotOptimise[(F64 | None)](boxed)
    DoNotOptimise.observe()
    match boxed
    | let v: F64 =>
      DoNotOptimise[F64](v)
      DoNotOptimise.observe()
    end

class iso _MatchU64OrNone is MicroBenchmark
  var _n: U64 = 0

  fun name(): String => "MatchU64OrNone"

  fun ref apply() =>
    _n = _n + 1
    let x: (U64 | None) = if (_n % 2) == 0 then _n else None end
    DoNotOptimise[(U64 | None)](x)
    DoNotOptimise.observe()
    match x
    | let v: U64 =>
      DoNotOptimise[U64](v)
      DoNotOptimise.observe()
    | None =>
      DoNotOptimise[None](None)
      DoNotOptimise.observe()
    end

class iso _IsBoxedU64 is MicroBenchmark
  var _n: U64 = 0

  fun name(): String => "IsBoxedU64"

  fun ref apply() =>
    _n = _n + 1
    let a: (U64 | None) = _n
    let b: (U64 | None) = _n
    DoNotOptimise[(U64 | None)](a)
    DoNotOptimise[(U64 | None)](b)
    DoNotOptimise.observe()
    let eq = a is b
    DoNotOptimise[Bool](eq)
    DoNotOptimise.observe()

class iso _DispatchStringable is MicroBenchmark
  var _n: U32 = 0

  fun name(): String => "DispatchStringable"

  fun ref apply() =>
    _n = _n + 1
    let s: Stringable = _n
    let result = s.string()
    DoNotOptimise[String](consume result)
    DoNotOptimise.observe()

class iso _MatchInLoop is MicroBenchmark
  let _items: Array[(U32 | None)]

  new iso create() =>
    _items = Array[(U32 | None)](32)
    var i: USize = 0
    while i < 32 do
      if (i % 3) == 0 then
        _items.push(None)
      else
        _items.push(U32(i.u32()))
      end
      i = i + 1
    end

  fun name(): String => "MatchInLoop"

  fun ref apply() =>
    var sum: U32 = 0
    var i: USize = 0
    while i < 32 do
      match try _items(i)? end
      | let v: U32 => sum = sum + v
      end
      i = i + 1
    end
    DoNotOptimise[U32](sum)
    DoNotOptimise.observe()
