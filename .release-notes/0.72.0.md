## Time the reach and paint passes with --pass-timings

The `--pass-timings` table and the `--pass-timings-json` file now include the reach and paint passes alongside the front-end passes. Reach and paint run over the whole program rather than a single package, so their rows show `<program>` in the package column.

## Speed up the reachability pass on large programs

The reachability pass now runs about three times faster on programs with many reachable types.

## Replace stdlib net package with ponylang/lori

The `net` package has been replaced with a new implementation based on [ponylang/lori](https://github.com/ponylang/lori). All existing networking code must be rewritten.

The old API used callback objects (`TCPConnectionNotify`, `TCPListenNotify`, `UDPNotify`) passed to framework-owned actors (`TCPConnection`, `TCPListener`, `UDPSocket`). The new API inverts this: your actors implement delegation traits (`TCPConnectionActor`, `TCPListenerActor`, `UDPSocketActor`) and hold a plain `TCPConnection` or `TCPListener` class that drives the I/O state machine. Lifecycle callbacks come through separate receiver traits (`ServerLifecycleEventReceiver`, `ClientLifecycleEventReceiver`, `UDPLifecycleEventReceiver`).

A `net/notifier` subpackage provides a callback-style API closer to the old pattern, where framework-owned actors handle I/O plumbing and your code implements notify traits (`ClientTCPConnectionNotify`, `ServerTCPConnectionNotify`, `TCPListenNotify`, `UDPSocketNotify`).

An SSL development library (OpenSSL or LibreSSL) is now a build dependency. CMake detects the installed library automatically. See BUILD.md for the per-distribution package names.

Before (TCP echo server, old API):

```pony
use "net"

actor Main
  new create(env: Env) =>
    TCPListener(TCPListenAuth(env.root), Listener(env.out))

class Listener is TCPListenNotify
  let _out: OutStream
  new create(out: OutStream) => _out = out
  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    recover Echoer(_out) end

class Echoer is TCPConnectionNotify
  let _out: OutStream
  new create(out: OutStream) => _out = out
  fun ref received(conn: TCPConnection ref, data: Array[U8] iso,
    times: USize): Bool
  =>
    conn.write(String.from_iso_array(consume data))
    true
```

After (TCP echo server, delegation API):

```pony
use "net"

actor Main
  new create(env: Env) =>
    EchoServer(TCPListenAuth(env.root), env.out)

actor EchoServer is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  let _server_auth: TCPServerAuth
  let _out: OutStream
  new create(auth: TCPListenAuth, out: OutStream) =>
    _out = out
    _server_auth = TCPServerAuth(auth)
    _tcp_listener = TCPListener(auth, "localhost", "7669", this)
  fun ref _listener(): TCPListener => _tcp_listener
  fun ref _on_accept(fd: U32): TCPConnectionActor =>
    Echoer(_server_auth, fd, _out)
  fun ref _on_listen_failure() => _out.print("listen failed")

actor Echoer is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream
  new create(auth: TCPServerAuth, fd: U32, out: OutStream) =>
    _out = out
    _tcp_connection = TCPConnection.server(auth, fd, this, this)
  fun ref _connection(): TCPConnection => _tcp_connection
  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _tcp_connection.send(consume data)
    KeepReading
  fun ref _on_start_failure(reason: StartFailureReason) => None
```

After (TCP echo server, notifier API):

```pony
use "net"
use notifier = "net/notifier"

actor Main
  new create(env: Env) =>
    notifier.TCPListener(
      TCPListenAuth(env.root),
      recover EchoListenNotify(env.out) end,
      "localhost", "7669")

class EchoListenNotify is notifier.TCPListenNotify
  let _out: OutStream
  new create(out: OutStream) => _out = out
  fun ref on_not_listening(listen: notifier.TCPListener ref) =>
    _out.print("listen failed")
  fun ref on_connected(listen: notifier.TCPListener ref):
    notifier.ServerTCPConnectionNotify iso^
  =>
    recover EchoNotify(_out) end

class EchoNotify is notifier.ServerTCPConnectionNotify
  let _out: OutStream
  new create(out: OutStream) => _out = out
  fun ref on_start_failure(conn: notifier.ServerTCPConnection ref,
    reason: StartFailureReason) => None
  fun ref on_received(conn: notifier.ServerTCPConnection ref,
    data: Array[U8] iso): ReadAction
  =>
    conn.write(consume data)
    KeepReading
```

## Change TCPBackend.connect to return Array[AsioEventID]

`TCPBackend.connect` now returns `Array[AsioEventID]` instead of `U32`. Each element is the ASIO event handle for one connection attempt. The connection uses these handles to cancel pending Happy Eyeballs attempts on hard close, instead of tracking a count.

Custom `TCPBackend` implementations must update the return type and return the actual events rather than a count.

Before:

```pony
fun ref connect(the_actor: AsioEventNotify, host: String, port: String,
  from: String, asio_flags: U32, ip_version: IPVersion): U32
=>
  // ...
  3 // count of connection attempts
```

After:

```pony
fun ref connect(the_actor: AsioEventNotify, host: String, port: String,
  from: String, asio_flags: U32, ip_version: IPVersion): Array[AsioEventID]
=>
  let events = Array[AsioEventID]
  // ... create sockets and ASIO events ...
  events
```
