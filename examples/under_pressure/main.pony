"""
This is an example of you, as a Pony user, can participate in Pony's built-in
backpressure system. This program starts up and connects to a program listening
on localhost port 7669. It then starts sending large chunks of data. If it
experiences backpressure, it uses the `Backpressure` primitive to inform the
Pony runtime to start applying backpressure to any actor sending messages to
the `TCPConnection`. In our program, that is a `TimerNotify` instance called
`Send`.

When backpressure is applied, `Send` sending a message to our `TCPConnection`
will cause it to not be scheduled again until our `TCPConnection` releases
backpressure.

To give this a try, do the following on any supported Unix* platform:

## Start a data receiver

```bash
nc -l 127.0.0.1 7669 >> /dev/null
```

## Get the pid of data receiver

This will vary slightly by OS.

## Start this program

```bash
./under_pressure
```

## Suspend the data receiver

```bash
kill -SIGSTOP DATA_RECEIVER_PID
```

## Watch backpressure be applied

Shortly after you suspend the data receiver, this application should print
"Experiencing backpressure!". If you check out CPU usage, you'll notice that
usage will be very low. No actors are running.

## Resume the data receiver

```bash
kill -SIGCONT DATA_RECEIVER_PID
```

## Watch backpressure get released

After resuming the data receiver, it will start reading data again, once
it drains to OS network buffer, this program will be informed that it
is no longer experiencing backpressure. When backpressure is released, this
program will print "Releasing backpressure!" and start sending data again. CPU
usage by this program will return to normal.
"""
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



