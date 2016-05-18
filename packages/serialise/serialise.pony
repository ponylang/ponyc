"""
# Serialise package

This package provides support for serialising and deserialising arbitrary data
structures.
"""

primitive SerialiseAuth
  """
  This is a capability that allows the holder to serialise objects. It does not
  allow the holder to examine serialised data or to deserialise objects.
  """
  new create(auth: AmbientAuth) =>
    None

primitive DeserialiseAuth
  """
  This is a capability token that allows the holder to deserialise objects. It
  does not allow the holder to serialise objects or examine serialised.
  """
  new create(auth: AmbientAuth) =>
    None

primitive OutputSerialisedAuth
  """
  This is a capability token that allows the holder to examine serialised data.
  This should only be provided to types that need to write serialised data to
  some output stream, such as a file or socket. A type with the SerialiseAuth
  capability should usually not also have OutputSerialisedAuth, as the
  combination gives the holder the ability to examine the bitwise contents of
  any object it has a reference to.
  """
  new create(auth: AmbientAuth) =>
    None

primitive InputSerialisedAuth
  """
  This is a capability token that allows the holder to treat data arbitrary
  bytes as serialised data. This is the most dangerous capability, as currently
  it is possible for a malformed chunk of data to crash your program if it is
  deserialised.
  """
  new create(auth: AmbientAuth) =>
    None

class val Serialised
  """
  This represents serialised data. How it can be used depends on the other
  capabilities a caller holds.
  """
  let _data: Array[U8] val

  new create(auth: SerialiseAuth, data: Any box) ? =>
    """
    A caller with SerialiseAuth can create serialised data from any object.
    """
    let r = recover Array[U8] end
    @pony_serialise[None](@pony_ctx[Pointer[None]](), data, r) ?
    _data = consume r

  new input(auth: InputSerialisedAuth, data: Array[U8] val) =>
    """
    A caller with InputSerialisedAuth can create serialised data from any
    arbitrary set of bytes. It is the caller's responsibility to ensure that
    the data is in fact well-formed serialised data. This is currently the most
    dangerous method, as there is currently no way to check validity at
    runtime.
    """
    _data = data

  fun apply(auth: DeserialiseAuth): Any iso^ ? =>
    """
    A caller with DeserialiseAuth can create an object graph from serialised
    data.
    """
    @pony_deserialise[Any iso^](@pony_ctx[Pointer[None]](), _data) ?

  fun output(auth: OutputSerialisedAuth): Array[U8] val =>
    """
    A caller with OutputSerialisedAuth can gain access to the underlying bytes
    that contain the serialised data. This can be used to write those bytes to,
    for example, a file or socket.
    """
    _data
