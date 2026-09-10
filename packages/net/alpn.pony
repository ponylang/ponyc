type ALPNProtocolName is String val
  """
  The name of an application protocol, as it travels over the wire in the ALPN
  extension. Between 1 and 255 bytes.
  """

primitive ALPNFatal
  """
  Select no protocol and fail the handshake. OpenSSL sends the peer a fatal
  alert.
  """

primitive ALPNNoAck
  """
  Select no protocol and let the handshake finish. The connection carries no
  negotiated protocol.
  """

primitive ALPNWarning
  """
  Select no protocol and let the handshake finish. OpenSSL sends the peer a
  warning alert first.
  """

type ALPNMatchResult is (ALPNProtocolName | ALPNNoAck | ALPNWarning | ALPNFatal)
  """
  What an `ALPNProtocolResolver` gives back: the protocol it chose, or one of
  the three ways to choose none.
  """

type _ALPNSelectCallback is @{(
  Pointer[_SSL] tag,
  Pointer[Pointer[U8] tag] tag,
  Pointer[U8] tag,
  Pointer[U8] box,
  U32,
  ALPNProtocolResolver val)
  : I32}

primitive _ALPNMatchResultCode
  """
  The `SSL_TLSEXT_ERR_*` values an ALPN select callback returns to OpenSSL.
  """
  fun ok(): I32 => 0
  fun warning(): I32 => 1
  fun fatal(): I32 => 2
  fun no_ack(): I32 => 3

primitive _ALPNProtocolList
  fun from_array(protocols: Array[String] box): String ? =>
    """
    Try to pack the protocol names in `protocols` into a *protocol name list*
    """
    if protocols.size() == 0 then
      error
    end

    let list = recover trn String end

    for proto in protocols.values() do
      let len = proto.size()
      if (len == 0) or (len > 255) then error end

      list.push(U8.from[USize](len))
      list.append(proto)
    end

    list

  fun offset_of(protocol_list: String box, name: String box): USize ? =>
    """
    The offset within `protocol_list` of the bytes of `name`.

    `protocol_list` is a *protocol name list*. Raises an error when the list is
    malformed, or when none of the names in it is `name`.

    The length prefix is what makes this an exact match rather than a search:
    `"h2"` is not found in a list whose only name is `"h2c"`.
    """
    let size = name.size()
    var index = USize(0)

    while index < protocol_list.size() do
      let len = USize.from[U8](protocol_list(index)?)
      if len == 0 then error end

      let start = index + 1
      if (start + len) > protocol_list.size() then error end

      if (len == size) and protocol_list.at(name, start.isize()) then
        return start
      end

      index = start + len
    end

    error

  fun to_array(protocol_list: String box): Array[ALPNProtocolName] val ? =>
    """
    Try to unpack a *protocol name list* into an `Array[String]`
    """
    let arr = recover trn Array[ALPNProtocolName] end

    var index = USize(1)
    var remain = try protocol_list(0)? else error end
    var buf = recover trn String end

    if remain == 0 then error end

    while index < protocol_list.size() do
      let ch = try protocol_list(index)? else error end
      if remain > 0 then
        buf.push(ch)
        remain = remain - 1
      end

      if remain == 0 then
        let final_protocol: String = buf = recover String end
        arr.push(final_protocol)

        let has_next_char = index < (protocol_list.size() - 1)
        if has_next_char then
          remain = try protocol_list(index + 1)? else error end
          if remain == 0 then error end
          index = index + 1
        end
      end
      index = index + 1
    end

    if remain > 0 then error end
    arr
