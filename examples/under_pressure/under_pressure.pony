  // Here we have a TCPConnectionNotify that upon construction
  // is given a tag to a "Coordinator". Any actors that want to
  // "publish" to this connection should register with the
  // coordinator. This allows the notifier to inform the coordinator
  // that backpressure has been applied and then it in turn can
  // notify senders who could then pause sending.

use "backpressure"
use "collections"
use "net"
use "time"

class SlowDown is TCPConnectionNotify
  let _auth: BackpressureAuth
  let _out: StdStream

  new iso create(auth: BackpressureAuth, out: StdStream) =>
    _auth = auth
    _out = out

  fun ref throttled(connection: TCPConnection ref) =>
    _out.print("Experiencing backpressure!")
    Backpressure(_auth)

  fun ref unthrottled(connection: TCPConnection ref) =>
    _out.print("Releasing backpressure!")
    Backpressure.release(_auth)

  fun ref connect_failed(conn: TCPConnection ref) =>
    None

class Send is TimerNotify
  let _sending_actor: TCPConnection

  new iso create(sending_actor: TCPConnection) =>
    _sending_actor = sending_actor

  fun ref apply(timer: Timer, count: U64): Bool =>
    let data = recover val Array[U8].init(72, 1024) end
    _sending_actor.write(data)
    _sending_actor.write("hi\n")
    true

actor Main
  new create(env: Env) =>
    try
      let auth = env.root as AmbientAuth
      let socket = TCPConnection(auth, recover SlowDown(auth, env.out) end,
        "", "7669")

      let timers = Timers
      let t = Timer(Send(socket), 0, 5_000_000)
      timers(consume t)
    end



