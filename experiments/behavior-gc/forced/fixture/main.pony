use @probe_setup[None](allocate: Pointer[None], read: Pointer[None])
use @probe_observe[None](cell: Cell)
use @probe_check[None](kept: Cell, replacement: Cell, expected: U64)
use @pony_exitcode[None](code: I32)

class \nodoc\ Cell
  var value: U64

  new create(n: U64) => value = n

actor \nodoc\ Probe
  be measure(n: U64) =>
    let kept = Cell(n)
    let replacement = Cell(211)
    @probe_check(kept, replacement, n)

actor \nodoc\ Main
  new create(env: Env) =>
    @pony_exitcode(1)
    @probe_setup(addressof make_temporary, addressof read_cell)
    Probe.measure(137)

  fun @make_temporary(): U64 =>
    @probe_observe(Cell(83))
    83

  fun @read_cell(cell: Cell box): U64 => cell.value
