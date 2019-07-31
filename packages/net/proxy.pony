
interface Proxy
  fun apply(wrap: TCPConnectionNotify iso): TCPConnectionNotify iso^

class val NoProxy is Proxy
  """
  Default implementation of a proxy that does not alter the supplied `TCPConnectionNotify`.

  ```pony
  actor MyClient
    new create(host: String, service: String, proxy: Proxy = NoProxy) =>
      let conn: TCPConnection = TCPConnection.create(
        env.root as AmbientAuth,
        proxy.apply(MyConnectionNotify.create()),
        "localhost",
        "80"
      )
  ```
  """
  fun apply(wrap: TCPConnectionNotify iso): TCPConnectionNotify iso^ => wrap
