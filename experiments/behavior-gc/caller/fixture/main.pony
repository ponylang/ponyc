use @caller_garbage[None](cell: Cell)
use @caller_poll[None]()
use @caller_leave_phase[None]()
use @caller_addresses[None](kept: Cell, fresh: Cell)
use @caller_result[None](before: U64, after: U64, fresh_value: U64)
use @pony_exitcode[None](code: I32)

class \nodoc\ Cell
  var value: U64

  new create(n: U64) => value = n

actor \nodoc\ Probe
  new create(n: U64) => measure(n)

  be measure(n: U64) =>
    let kept = Cell(n)
    phase()
    let replacement = Cell(211)
    @caller_addresses(kept, replacement)
    let before = kept.value
    kept.value = before + 5
    @caller_result(before, kept.value, replacement.value)

  fun \noinline\ phase() =>
    allocate_garbage()
    @caller_poll()
    @caller_leave_phase()

  fun \noinline\ allocate_garbage() =>
    var i: U64 = 0
    while i < 1024 do
      @caller_garbage(Cell(i))
      i = i + 1
    end

actor \nodoc\ Main
  new create(env: Env) =>
    @pony_exitcode(1)
    Probe(137)
