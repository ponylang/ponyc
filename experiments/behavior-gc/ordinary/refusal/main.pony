use @pony_exitcode[None](code: I32)
use @puts[I32](s: Pointer[U8] tag)

class \nodoc\ val Payload
  let value: USize

  new val create(n: USize) => value = n

class \nodoc\ Resource
  let value: USize = 79

  fun _final() => @puts("finalized".cstring())

actor \nodoc\ Main
  new create(env: Env) =>
    @pony_exitcode(1)
    let mode = try env.args(1)? else "" end
    if mode == "foreign" then
      Worker(2, Payload(91))
    elseif mode == "local" then
      Worker(3, None)
    elseif mode == "final" then
      Worker(1, None)
    else
      Worker(0, None)
    end

actor \nodoc\ Worker
  let _mode: USize
  let _remote: (Payload | None)
  var _resource: (Resource | None) = None
  var _data: Array[U8] = Array[U8]

  new create(mode: USize, remote: (Payload | None)) =>
    _mode = mode
    _remote = remote
    run_small()

  be run_small() =>
    allocate()
    verify()

  fun ref allocate() =>
    _data = Array[U8].init(19, 65536)
    if _mode == 1 then _resource = Resource end
    if _mode == 3 then check_later(Payload(_data.size())) end

  be check_later(item: Payload) =>
    if item.value == 65536 then @puts("queued".cstring()) end

  fun verify() =>
    try
      if (_data(65535)? == 19) and
        ((_mode != 1) or ((_resource as Resource box).value == 79)) and
        ((_mode != 2) or ((_remote as Payload).value == 91))
      then
        @puts("verified".cstring())
        @pony_exitcode(0)
      end
    end
