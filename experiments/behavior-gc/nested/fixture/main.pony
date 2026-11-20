use @nested_garbage[None](cell: Cell)
use @nested_poll[None]()
use @nested_leave_phase[None]()
use @nested_fresh[None](fresh: Cell)
use @nested_value[None](level: U8, cell: Cell, old: U64, value: U64)
use @pony_exitcode[None](code: I32)

class \nodoc\ Cell
  var value: U64

  new create(n: U64) => value = n

actor \nodoc\ Probe
  new create(n: U64) => measure(n)

  be measure(n: U64) =>
    let outer = Cell(n)
    middle()
    let old = outer.value
    outer.value = old + 5
    @nested_value(0, outer, old, outer.value)

  fun \noinline\ ref middle() =>
    let inner = Cell(271)
    phase()
    let fresh = Cell(211)
    @nested_fresh(fresh)
    let old = inner.value
    inner.value = old + 7
    @nested_value(1, inner, old, inner.value)

  fun \noinline\ phase() =>
    allocate_garbage()
    @nested_poll()
    @nested_leave_phase()

  fun \noinline\ allocate_garbage() =>
    var i: U64 = 0
    while i < 1024 do
      @nested_garbage(Cell(i))
      i = i + 1
    end

actor \nodoc\ Main
  new create(env: Env) =>
    @pony_exitcode(1)
    Probe(137)
