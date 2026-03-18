use "lib:codegen-trace-additional"

use @raw_cast[B ref](obj: B tag)
use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @pony_exitcode[None](code: I32)

class A

interface Readable
  fun read(): None

interface Writable
  fun write(): None

class B is (Readable & Writable)
  let a: A = A
  fun read(): None => None
  fun write(): None => None

trait tag Receiver
  be receive(b: (Readable iso & Writable iso))

actor Main is Receiver
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    let r: Receiver = this
    r.receive(recover B end)
    map_before = @gc_local_snapshot(this)

  be receive(b: (Readable val & Writable val)) =>
    match b
    | let b': B val =>
      let b'' = @raw_cast(b')
      let map_after = @gc_local(this)
      let ok = @objectmap_has_object_rc(map_before, b'', USize(1)) and
        @objectmap_has_object_rc(map_before, b''.a, USize(1)) and
        @objectmap_has_object_rc(map_after, b'', USize(0)) and
        @objectmap_has_object_rc(map_after, b''.a, USize(0))
      @gc_local_snapshot_destroy(map_before)
      @pony_exitcode(I32(if ok then 1 else 0 end))
    end
