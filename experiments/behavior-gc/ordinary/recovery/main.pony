use @puts[I32](s: Pointer[U8] tag)
use @pony_exitcode[None](code: I32)

class \nodoc\ Resource
  let value: USize = 137

  fun _final() => @puts("finalized".cstring())

actor \nodoc\ Main
  new create(env: Env) =>
    @pony_exitcode(1)
    Worker

actor \nodoc\ Worker
  var _resource: (Resource | None) = None
  var _bytes: Array[U8] = Array[U8]
  var _step: USize = 0

  new create() =>
    first()
    run_small()
    last()

  be first() =>
    allocate_resource()
    inspect()
    release_resource()

  fun ref allocate_resource() =>
    _resource = Resource
    _bytes = Array[U8].init(17, 65536)

  fun ref inspect() =>
    try
      if ((_resource as Resource).value == 137) and (_bytes(65535)? == 17) then
        _step = 1
      end
    end

  fun ref release_resource() =>
    _resource = None

  be run_small() =>
    allocate_plain()
    inspect_plain()

  fun ref allocate_plain() =>
    _bytes = Array[U8].init(211, 524288)

  fun ref inspect_plain() =>
    try
      if (_step == 1) and (_bytes(524287)? == 211) then _step = 2 end
    end

  be last() =>
    if _step == 2 then
      @puts("verified".cstring())
      @pony_exitcode(0)
    end
