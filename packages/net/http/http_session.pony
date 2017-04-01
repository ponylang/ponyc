interface tag HTTPSession
  """
  An HTTP Session is the external API to the communication link
  between client and server.  A session can only transfer one message
  at a time in each direction.  The client and server each have their
  own ways of implementing this interface, but to application code (either
  in the client or in the server 'back end') this interface provides a
  common view of how information is passed *into* the `net/http` package.
  """
  be apply(payload: Payload val)
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

  be dispose()
    """
    Close the connection from this end.
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

