"""
This example is a stress test of Pony messaging and mailbox usage.
All X Mailers send to a single actor, Main, attempting to overload it's
mailbox. This is a degenerate condition, the more actors you have sending
to Main (and the more messages they send), the more memory will be used.

Run this and watch the process memory usage grow as actor Main gets swamped.

Also note, it finishes reasonably quickly for how many messages that single
actor ends up processing.
"""
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
