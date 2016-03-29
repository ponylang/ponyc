"""
Causes uncontrolled memory usage with 4 threads
"""
use "collections"

actor Ping
  var _passes: U32
  var _total_passes: U32 = 0

  new create(passes: U32) =>
    _passes = passes

  be ping(partner: Pong) =>
    if (_total_passes < _passes) then
      partner.pong(this)
    end
    _total_passes = _total_passes + 1

actor Pong
  be pong(partner: Ping) =>
    partner.ping(this)


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
      let ping = Ping(_pass)
      let pong' = Pong
      ping.ping(pong')
    end

  fun ref parse_args() ? =>
    _size = _env.args(1).u32()
    _pass = _env.args(2).u32()

  fun ref usage() =>
    _env.out.print(
      """
      mailbox OPTIONS
        N   number of sending actors
        M   number of messages to pass from each sender to the receiver
      """
      )
