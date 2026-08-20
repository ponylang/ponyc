
interface Proxy
  """
  Wraps a `TCPConnectionNotify` to intercept connection setup.
  """
  fun apply(wrap: TCPConnectionNotify iso): TCPConnectionNotify iso^
    """
    Apply the proxy to the given notifier.
    """

class val NoProxy is Proxy
  """
  Default implementation of a proxy that does not alter the supplied
  `TCPConnectionNotify`.

  ```pony
  actor MyClient
    new create(host: String, service: String, proxy: Proxy = NoProxy) =>
      let conn: TCPConnection = TCPConnection.create(
        TCPConnectAuth(env.root),
        proxy.apply(MyConnectionNotify.create()),
        "localhost",
        "80")
  ```
  """
  fun apply(wrap: TCPConnectionNotify iso): TCPConnectionNotify iso^ => wrap
