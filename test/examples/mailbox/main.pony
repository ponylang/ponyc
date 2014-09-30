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
    var i = 1

    while i < _env.args.length() do
      var arg = _env.args(i)

      match arg
      | "--size" =>
        _size = _env.args(i + 1).u32()
      | "--pass" =>
        _pass = _env.args(i + 1).u32()
      else
        error
      end

      i = i + 1
    end

  fun ref usage() =>
    _env.stdout.print(
      """
      mailbox OPTIONS
        --size N   number of sending actors
        --pass N   number of messages to pass from each sender to the receiver
      """
      )
