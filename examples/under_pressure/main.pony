use "backpressure"
use "collections"
use "net"
use "time"
use @printf[I32](fmt: Pointer[U8] tag, ...)

class SlowDown is TCPConnectionNotify
  """
  Applies and releases runtime backpressure in response to
  TCP throttling events.
  """
  let _auth: ApplyReleaseBackpressureAuth
  let _out: OutStream

  new iso create(
    auth: ApplyReleaseBackpressureAuth,
    out: OutStream)
  =>
    _auth = auth
    _out = out

  fun ref connected(conn: TCPConnection ref) =>
    """
    Query and set socket options, then print the results.
    """
    let bufsiz: U32 = 5000

    @printf(
      "Querying and setting socket options:\n"
        .cstring())
    @printf(
      "\tgetsockopt so_error = %d\n".cstring(),
      conn.get_so_error()._2)
    @printf(
      "\tgetsockopt get_tcp_nodelay = %d\n".cstring(),
      conn.get_tcp_nodelay()._2)
    @printf(
      ("\tgetsockopt set_tcp_nodelay(true) " +
        "return value = %d\n")
        .cstring(),
      conn.set_tcp_nodelay(true))
    @printf(
      "\tgetsockopt get_tcp_nodelay = %d\n".cstring(),
      conn.get_tcp_nodelay()._2)

    @printf(
      "\tgetsockopt rcvbuf = %d\n".cstring(),
      conn.get_so_rcvbuf()._2)
    @printf(
      "\tgetsockopt sndbuf = %d\n".cstring(),
      conn.get_so_sndbuf()._2)
    @printf(
      "\tsetsockopt rcvbuf %d return was %d\n"
        .cstring(),
      bufsiz,
      conn.set_so_rcvbuf(bufsiz))
    @printf(
      "\tsetsockopt sndbuf %d return was %d\n"
        .cstring(),
      bufsiz,
      conn.set_so_rcvbuf(bufsiz))

  fun ref throttled(connection: TCPConnection ref) =>
    _out.print("Experiencing backpressure!")
    Backpressure(_auth)

  fun ref unthrottled(connection: TCPConnection ref) =>
    _out.print("Releasing backpressure!")
    Backpressure.release(_auth)

  fun ref closed(connection: TCPConnection ref) =>
    _out.print("Releasing backpressure if applied!")
    Backpressure.release(_auth)

  fun ref connect_failed(conn: TCPConnection ref) =>
    @printf("connect_failed\n".cstring())
    None

class Send is TimerNotify
  """
  Periodically writes data to the TCP connection.
  """
  let _sending_actor: TCPConnection

  new iso create(sending_actor: TCPConnection) =>
    _sending_actor = sending_actor

  fun ref apply(timer: Timer, count: U64): Bool =>
    """
    Write a chunk of data on each timer tick.
    """
    let data =
      recover val Array[U8].init(72, 16384) end
    _sending_actor.write(data)
    _sending_actor.write("hi\n")
    true

actor Main
  new create(env: Env) =>
    let socket =
      TCPConnection(
        TCPConnectAuth(env.root),
        recover
          SlowDown(
            ApplyReleaseBackpressureAuth(env.root),
            env.out)
        end,
        "",
        "7669")

    let timers = Timers
    let t = Timer(Send(socket), 0, 5_000_000)
    timers(consume t)
