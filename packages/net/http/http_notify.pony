"""
This package includes all the support functions necessary to build client
and server applications for the HTTP protocol.

The important interfaces an application needs to deal with are:

* [HTTPSession](net-http-HTTPSession), the API to an HTTP connection.

* [HTTPHandler](net-http-HTTPHandler), the interface to a handler you
need to write that will receive notifications from the `HTTPSession`.

* [HandlerFactory](net-http-HandlerFactory), the interface to a class you
need to write that creates instances of your `HTTPHandler`.

* [Payload](net-http-Payload), the class that represents a single HTTP
message, with its headers.

If you are writing a client, you will need to deal with the
[HTTPClient](net-http-HTTPClient) class.

If you are writing a server, you will need to deal with the
[HTTPServer](net-http-HTTPServer) class.

"""

interface HTTPSession
  """
  An HTTP Session is the external API to the communication link
  between client and server.  A session can only transfer one message
  at a time in each direction.  The client and server each have their
  own ways of implementing this interface, but to application code (either
  in the client or in the server 'back end') this interface provides a
  common view of how information is passed *into* the `net/http` package.
  """
  be apply( payload: Payload val)
    """
    Start sending a request or response.  The `Payload` must have all its
    essential fields filled in at this point, because ownership is being
    transferred to the session actor.  This begins an outbound message.
    """

  be finish()
    """
    Indicate that all *outbound* `add_chunk` calls have been made and
    submission of the HTTP message is complete.
    """

  be write(data: ByteSeq val)
    """
    Write raw byte stream to the outbound TCP connection.
    """

  be _mute()
    """
    Stop delivering *incoming* data to the handler.  This may not
    be effective instantly.
    """

  be _unmute()
    """
    Resume delivering incoming data to the handler.
    """

  be cancel(msg: Payload val)
    """
    Tell the session to stop sending an *outbound* message.
    """

  be _deliver(payload: Payload val)
    """
    The appropriate Payload Builder will call this from the `TCPConnection`
    actor to start delivery of a new *inbound* message.  If the `Payload`s
    `transfer_mode` is `OneshotTransfer`, this is the only notification 
    that will happen for the message.  Otherwise there will be one or more
    `_chunk` calls followed by a `_finish` call.
    """

  be _chunk(data: ByteSeq val)
    """
    Deliver a piece of *inbound* body data to the application `HTTPHandler`
    This is called by the PayloadBuilder.
    """

  be _finish()
    """
    Inidcates that the last *inbound* body chunk has been sent to
    `_chunk`.  This is called by the PayloadBuilder.
    """

interface HandlerFactory
  """
  The TCP connections that underlie HTTP sessions get created within
  the `net/http` package at times that the application code can not
  predict.  Yet, the application code has to provide custom hooks into
  these connections as they are created.  To accomplish this, the
  application code provides a `class` that implements this interface.

  The `HandlerFactory.apply` method will be called when a new
  `HTTPSession` is created, giving the application a chance to create
  an instance of its own `HTTPHandler`.  This happens on both
  client and server ends.
  """
  fun apply(session: HTTPSession tag): HTTPHandler ref^
    """
    Called by the `HTTPSession` when it needs a new instance of the
    application's `HTTPHandler`.  It is suggested that the
    `session` value be passed to the constructor for the new
    `HTTPHandler` so that it is available for making
    `throttle` and `unthrottle` calls.
    """

interface HTTPHandler
  """
  This is the interface through which HTTP messages are delivered *to*
  application code.  On the server, this will be HTTP Requests (GET,
  HEAD, DELETE, POST, etc) sent from a client.  On the client, this will
  be the HTTP Responses coming back from the server.  The protocol is largely
  symmetrical and the same interface definition is used, though what
  processing happens behind the interface will of course vary.

  This interface delivers asynchronous events when receiving an HTTP
  message (called a `Payload`).  Calls to these methods are made in
  the context of the `HTTPSession` actor so most of them should be
  passing data on to a processing actor.

  Each `HTTPSession` must have a unique instance of the handler.
  """
  fun ref apply(payload: Payload val): Any => None
    """
    Notification of an incoming message.  On the client, these will
    be responses coming from the server.  On the server these will be requests
    coming from the client.  The `Payload` object carries HTTP headers
    and the method, URL, and status codes.

    Only one HTTP message will be processed at a time, and that starts
    with a call to this method.  This would be a good time to create
    an actor to deal with subsequent information pertaining to this
    message.
    """

  fun ref chunk(data: ByteSeq val) => None
    """
    Notification of incoming body data.  The body belongs to the most
    recent `Payload` delivered by an `apply` notification.
    """

  fun ref finished() => None
    """
    Notification that no more body chunks are coming.  Delivery of this HTTP
    message is complete.
    """

  fun ref cancelled() => None
    """
    Notification that the communication link has broken in the middle of
    transferring the payload.  Everything received so far should
    be discarded.  Any transmissions should be terminated.
    """

  fun ref throttled() => None
    """
    Notification that the session temporarily can not accept more data.
    """

  fun ref unthrottled() => None
    """
    Notification that the session can resume accepting data.
    """

  fun ref need_body() => None
    """
    Notification that the HTTPSession is ready for Stream or Chunked
    body data.
    """

