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
    try
      _size = _env.args(1).u32()
      _pass = _env.args(2).u32()
    else
      error
    end

  fun ref usage() =>
    _env.out.print(
      """
      mailbox OPTIONS
        N   number of sending actors
        M   number of messages to pass from each sender to the receiver
      """
      )
