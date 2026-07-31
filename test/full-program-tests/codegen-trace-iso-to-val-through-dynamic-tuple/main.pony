use "lib:codegen-trace-additional"

use @raw_cast[B ref](obj: B tag)
use @gc_local[Pointer[None]](target: Main)
use @gc_local_snapshot[Pointer[None]](target: Main)
use @gc_local_snapshot_destroy[None](obj_map: Pointer[None])
use @objectmap_has_object_rc[Bool](obj_map: Pointer[None], obj: Any tag, rc: USize)
use @pony_exitcode[None](code: I32)

class A

class B
  let a: A = A

class val C

trait tag Receiver
  be receive(t: ((B iso, U64) | C val))

actor Main is Receiver
  var map_before: Pointer[None] = Pointer[None]

  new create(env: Env) =>
    let r: Receiver = this
    r.receive((recover B end, U64(7)))
    map_before = @gc_local_snapshot(this)

  be receive(t: ((B val, U64) | C val)) =>
    match t
    | (let b: B val, let _: U64) =>
      let b' = @raw_cast(b)
      let map_after = @gc_local(this)
      let sender_b = @objectmap_has_object_rc(map_before, b', USize(1))
      let sender_a = @objectmap_has_object_rc(map_before, b'.a, USize(1))
      let receiver_b = @objectmap_has_object_rc(map_after, b', USize(0))
      let receiver_a = @objectmap_has_object_rc(map_after, b'.a, USize(0))
      @gc_local_snapshot_destroy(map_before)
      @pony_exitcode(
        I32(if sender_b and sender_a and receiver_b and receiver_a then
          1
        else
          0
        end))
    end
