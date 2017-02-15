use "collections"
use "net"
use "net/ssl"

actor HTTPServer
  """
  Runs an HTTP server.

  ### Server operation

  Information flow into the Server is as follows:

  1. `Server` listens for incoming TCP connections.

  2. `RequestBuilder` is the notification class for new connections.  It creates
  a `ServerConnection` actor and receives all the raw data from TCP.  It
  uses the `HTTPParser` to assemble complete `Payload` objects which are
  passed off to the `ServerConnection`.

  3. The `ServerConnection` actor deals with *completely formed* requests
  that have been parsed by the `HTTPParser`.  This is where requests get
  dispatched to the caller-provided Handler.

  With streaming content, dispatch to the application's back end Handler
  has to happen *before* all of the body has been received.  This has
  to be carefully choreographed because a `Payload` is an `iso` object and can
  only belong to one actor at a time, yet the `RequestBuilder` is running
  within the `TCPConnection` actor while the `RequestHandler` is running under
  the `ServerConnection` actor.  Each incoming bufferful of body data,
  a `ByteSeq val`, is handed off to `ServerConnection`, to be passed on
  to the back end Handler.

  1. It turns out that the issues in sending a request and a response
  are the same, as are the issues in receiving them.  Therefore the same
  notification interface, `HTTPHandler` is used on both ends, and the same
  sending interface `HTTPSession` is used.  This makes the code easier
  to read as well.

  1. `HTTPHandler.apply()` will be the way the client/server is informed of a
  new response/request message.  All of the headers will be present so that the
  request can be dispatched for correct processing.  Subsequent calls to a new
  function `HTTPHandler.chunk` will provide the body data, if any.  This
  stream will be terminated by a call to the new function
  `HTTPHandler.finished`.

  2. Pipelining of requests is to optimize the transmission of requests over
  slow links (such as over satellites), not to cause simultaneous execution
  on the server within one session.  Multiple received simple requests (`GET`,
  `HEAD`, and `OPTIONS`) are queued in the server and passed to the back end
  application one at a time.  If a client wants true parallel execution of
  requests, it should use multiple sessions (which many browsers actually
  do already).

  Since processing of a streaming response can take a relatively long time,
  acting on additional requests in the meantime does nothing but use up memory
  since responses would have to be queued. And if the server is being used to
  stream media, it is possible that these additional requests will themselves
  generate large responses.   Instead we will just let the requests queue up
  until a maximum queue length is reached (a small number) at which point we
  will back-pressure the inbound TCP stream.
  """
  let _notify: ServerNotify
  var _handler_maker: HandlerFactory val
  var _logger: Logger
  let _reversedns: (DNSLookupAuth | None)
  let _sslctx: (SSLContext | None)
  let _listen: TCPListener
  var _address: NetAddress
  var _dirty_routes: Bool = false
  let _sessions: SetIs[TCPConnection tag] = SetIs[TCPConnection tag]

  new create(auth: TCPListenerAuth, notify: ServerNotify iso,
    handler: HandlerFactory val, logger: Logger = DiscardLog,
    host: String = "", service: String = "0", limit: USize = 0,
    sslctx: (SSLContext | None) = None,
    reversedns: (DNSLookupAuth | None) = None)
  =>
    """
    Create a server bound to the given host and service.  To do this we
    listen for incoming TCP connections, with a notification handler
    that will create a server session actor for each one.
    """
    _notify = consume notify
    _handler_maker = handler
    _logger = logger
    _reversedns = reversedns
    _sslctx = sslctx

    _listen = TCPListener(auth,
        _ServerListener(this, sslctx, _handler_maker, _logger, _reversedns),
        host, service, limit)

    _address = recover NetAddress end

  be register_session(conn: TCPConnection) =>
    _sessions.set(conn)

  be unregister_session(conn: TCPConnection) =>
    _sessions.unset(conn)

  be set_handler(handler: HandlerFactory val) =>
    """
    Replace the request handler.
    """
    _handler_maker = handler
    _listen.set_notify(
      _ServerListener(this, _sslctx, _handler_maker, _logger, _reversedns))

  be set_logger(logger: Logger) =>
    """
    Replace the logger.
    """
    _logger = logger
    _listen.set_notify(
      _ServerListener(this, _sslctx, _handler_maker, _logger, _reversedns))

  be dispose() =>
    """
    Shut down the server gracefully.  To do this we have to eliminate
    and source of further inputs.  So we stop listening for new incoming
    TCP connections, and close any that still exist.
    """
    _listen.dispose()
    for conn in _sessions.values() do
      conn.dispose()
    end

  fun local_address(): NetAddress =>
    """
    Returns the locally bound address.
    """
    _address

  be _listening(address: NetAddress) =>
    """
    Called when we are listening.
    """
    _address = address
    _notify.listening(this)

  be _not_listening() =>
    """
    Called when we fail to listen.
    """
    _notify.not_listening(this)

  be _closed() =>
    """
    Called when we stop listening.
    """
    _notify.closed(this)
