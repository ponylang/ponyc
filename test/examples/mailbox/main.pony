use "collections"

actor Mailer
  be ping(receiver: Main, pass: U32) =>
    for i in Range[U32](0, pass) do
      receiver.pong()
    end

actor Main
  var _env: Env
  var _size: U32 = 3
  var _pass: U32 = 0
  var _pongs: U64 = 0

  new create(env: Env) =>
    _env = env

    try
      parse_args()
      start_messaging()
    else
      usage()
    end

  be pong() =>
    _pongs = _pongs + 1

  fun ref start_messaging() =>
    for i in Range[U32](0, _size) do
      Mailer.ping(this, _pass)
    end

  fun ref parse_args() ? =>
    var i: U64 = 1

    while i < _env.args.size() do
      var arg = _env.args(i)
      var value = _env.args(i + 1)
      i = i + 2

      match arg
      | "--size" =>
        _size = value.u32()
      | "--pass" =>
        _pass = value.u32()
      else
        error
      end
    end

  fun ref usage() =>
    _env.out.println(
      """
      mailbox OPTIONS
        --size N   number of sending actors
        --pass N   number of messages to pass from each sender to the receiver
      """
      )
