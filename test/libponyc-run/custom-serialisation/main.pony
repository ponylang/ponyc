use "serialise"
use "lib:custom-serialisation-additional"

use @pony_exitcode[None](code: I32)
use @test_custom_serialisation_get_object[Pointer[U8] ref]()
use @test_custom_serialisation_free_object[None](p: Pointer[U8] tag)
use @test_custom_serialisation_serialise[None](p: Pointer[U8] tag, bytes: Pointer[U8] ref)
use @test_custom_serialisation_deserialise[Pointer[U8] ref](byres: Pointer[U8] ref)
use @test_custom_serialisation_compare[U8](p1: Pointer[U8] tag, p2: Pointer[U8] tag)

class _Custom
  let s1: String = "abc"
  var p: Pointer[U8]
  let s2: String = "efg"

  new create() =>
    p = @test_custom_serialisation_get_object()

  fun _final() =>
    @test_custom_serialisation_free_object(p)

  fun _serialise_space(): USize =>
    8

  fun _serialise(bytes: Pointer[U8]) =>
    @test_custom_serialisation_serialise(p, bytes)

  fun ref _deserialise(bytes: Pointer[U8]) =>
    p = @test_custom_serialisation_deserialise(bytes)

  fun eq(c: _Custom): Bool =>
    (@test_custom_serialisation_compare(this.p, c.p) == 1) and
    (this.s1 == c.s1)
      and (this.s2 == c.s2)

actor Main
  new create(env: Env) =>
    try
      let serialise = SerialiseAuth(env.root)
      let deserialise = DeserialiseAuth(env.root)

      let x: _Custom = _Custom
      let sx = Serialised(serialise, x)?
      let yd: Any ref = sx(deserialise)?
      let y = yd as _Custom
      let r: I32 = if (x isnt y) and (x == y) then
        1
      else
        0
      end
      @pony_exitcode(r)
    end
