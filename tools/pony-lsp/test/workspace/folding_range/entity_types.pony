primitive \nodoc\ P
  fun value(): U32 =>
    42

struct \nodoc\ S
  var x: U32 = 0

  new create() => None

  fun get(): U32 =>
    x

actor \nodoc\ A
  var _n: U32 = 0

  new create() => _n = 0

  be tick() =>
    _n = _n + 1

  fun count(): U32 =>
    _n

trait \nodoc\ T
  fun \nodoc\ required(): U32

  fun doubled(): U32 =>
    required() * 2

interface \nodoc\ EntityTypes
  fun \nodoc\ required(): U32

  fun tripled(): U32 =>
    required() * 3
