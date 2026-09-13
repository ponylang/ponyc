## Add AsioBackend trait for mockable ASIO operations

`TCPConnection`, `TCPListener`, and the related traits and type aliases now take a second type parameter `Asio: AsioBackend ref = RuntimeAsio` that controls which ASIO backend the connection uses. The default `RuntimeAsio` delegates to the runtime's `PonyAsio` calls, so existing code that omits the parameter compiles without changes.

The new `AsioBackend` trait covers the full `PonyAsio` operation surface. `RuntimeAsio` is the production implementation.

This parallels the existing `TCPBackend` / `RuntimeBackend` seam: where `TCPBackend` lets tests replace socket I/O, `AsioBackend` lets tests replace timer creation, event subscription, and readability/writeability signaling. Together the two seams make it possible to test connection logic — including idle-timer resets on send and receive — without real sockets or real timers.

To supply a custom ASIO backend:

```pony
actor MyServer is (TCPConnectionActor[MyBackend, MyAsio]
  & ServerLifecycleEventReceiver[MyBackend, MyAsio])

  var _conn: TCPConnection[MyBackend, MyAsio] =
    TCPConnection[MyBackend, MyAsio].none()

  fun ref _connection(): TCPConnection[MyBackend, MyAsio] => _conn
```
