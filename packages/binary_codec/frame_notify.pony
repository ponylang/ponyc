interface ref FrameNotify[A: Any val]
  """
  Callback interface for `FrameDecoder`. Receives complete decoded values
  and errors from the streaming frame extraction loop.
  """

  fun ref frame_received(value: A)
    """
    Called when a complete frame has been decoded from the stream.
    """

  fun ref frame_error(err: MalformedData)
    """
    Called when the decoder encounters structurally invalid data.
    """
