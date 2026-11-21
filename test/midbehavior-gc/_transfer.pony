use @open[I32](path: Pointer[U8] tag, flags: I32, ...) if not windows
use @_open[I32](path: Pointer[U8] tag, flags: I32, ...) if windows
use @pony_os_errno[I32]()

actor \nodoc\ _Owner
  var _buffer: Array[U64] = Array[U64]

  new create() => _self_send()

  be _self_send() =>
    let data = recover iso Array[U64].init(24, 16) end
    _received(consume data)
    let reused = _reuses_storage()
    if reused then @pony_exitcode(24) end

  be _received(data: Array[U64] iso) =>
    try
      if data(15)? != 24 then @pony_exitcode(25) end
    else
      @pony_exitcode(25)
    end
    _send()

  fun \noinline\ _maybe_array(value: U64): (Array[U64] | None) =>
    if value > 0 then Array[U64].init(value, 1) end

  fun ref _match_loop(value: U64) =>
    match _maybe_array(value)
    | let captured: Array[U64] =>
      var i: U64 = 0
      repeat
        try
          if captured(0)? != (value + i) then @pony_exitcode(26) end
          captured(0)? = value + i + 1
        else
          @pony_exitcode(26)
        end
        let reused = _reuses_storage()
        if not reused then @pony_exitcode(27) end
        i = i + 1
      until i == 2 end
    else
      @pony_exitcode(28)
    end

  be _send() =>
    _match_loop(43)
    let saved = Array[U64].init(43, 1)
    try
      _raise_after_collection()?
      @pony_exitcode(17)
    else
      try
        if saved(0)? != 43 then @pony_exitcode(18) end
      else
        @pony_exitcode(19)
      end
    end
    let fd =
      ifdef windows then
        @_open("".cstring(), I32(0))
      else
        @open("".cstring(), I32(0))
      end
    let saved_errno = @pony_os_errno()
    let reused = _reuses_storage()
    if not reused then @pony_exitcode(12) end
    if (fd != -1) or (saved_errno == 0) or
      (@pony_os_errno() != saved_errno)
    then
      @pony_exitcode(23)
    end
    let data = recover iso Array[_Element].init(_Element(0), 8192) end
    _Writer._take(this, consume data)

  fun \noinline\ ref _raise_after_collection() ? =>
    let reused = _reuses_storage()
    if not reused then @pony_exitcode(20) end
    error

  be _while_sent(writer: _Writer) =>
    let reused = _reuses_storage()
    if reused then @pony_exitcode(13) end
    writer._check(this)

  be _after_release() =>
    let reused = _reuses_storage()
    if not reused then @pony_exitcode(29) end

  fun ref _reuses_storage(): Bool =>
    let ids = Array[USize]
    var reused = false
    var i: U64 = 0
    while i < 128 do
      _buffer = Array[U64].init(i, 8192)
      let id = digestof _buffer
      if ids.contains(id) then reused = true end
      ids.push(id)
      i = i + 1
    end
    reused

class \nodoc\ val _Element
  let value: U64

  new val create(value': U64) => value = value'

actor \nodoc\ _Writer
  var _data: Array[_Element]
  let _first: _Element = _Element(1)
  let _second: _Element = _Element(2)
  var _odd: Bool = false
  var _done: Bool = false

  new _take(owner: _Owner, data: Array[_Element] iso) =>
    _data = consume data
    _replace()
    owner._while_sent(this)
    _mutate()

  be _mutate() =>
    if not _done then
      _replace()
      _mutate()
    end

  fun ref _replace() =>
    _odd = not _odd
    let child = if _odd then _first else _second end
    try
      var i: USize = 0
      while i < _data.size() do
        _data(i)? = child
        i = i + 1
      end
    else
      @pony_exitcode(14)
    end

  be _check(owner: _Owner) =>
    _done = true
    let expected: U64 = if _odd then 1 else 2 end
    try
      var i: USize = 0
      while i < 8192 do
        if _data(i)?.value != expected then @pony_exitcode(15) end
        i = i + 1
      end
    else
      @pony_exitcode(16)
    end
    _data = Array[_Element]
    @pony_triggergc(@pony_ctx())
    _released(owner)

  be _released(owner: _Owner) =>
    owner._after_release()
