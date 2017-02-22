use "collections"
use "net"
use "format"

primitive ChunkedTransfer
primitive StreamTransfer
primitive OneshotTransfer

type TransferMode is (ChunkedTransfer | StreamTransfer | OneshotTransfer)

class trn Payload
  """
This class represent a single HTTP message, which can be either a
`request` or a `response`.

### Transfer Modes

HTTP provides two ways to encode the transmission of a message 'body',
of any size.  This package supports both of them:

2. **StreamTransfer**.  This is used for payload bodies where the exact length
is known in advance, including most transfers of files.  It is selected
by calling `Payload.set_length` with an integer bytecount.
Appication buffer sizes determine how much data is fed to the TCP connection
at once, but the total amount must match this size.

3. **ChunkedTransfer**.  This is used when the payload length can not be known
in advance, but can be large.   It is selected by calling `Payload.set_length`
with a parameter of `None`.  On the TCP link this mode can be detected because
there is no `Content-Length` header at all, being replaced by the
`Transfer-Encoding: chunked` header.  In addition, the message body is
separated into chunks, each with its own bytecount.  As with `StreamTransfer`
mode, transmission can be spread out over time with the difference that it is
the original data source that determines the chunk size.

If `Payload.set_length` is never called at all, a variation on
`StreamTransfer` called `OneshotTransfer` is used.  In this case, all of the
message body is placed into the message at once, using `Payload.add_chunk`
calls.  The size will be determined when the message is submitted for
transmission.  Care must be taken not to consume too much memory, especially
on a server where there can be multiple messages in transit at once.

The type of transfer being used by an incoming message can be determined
from its `transfer_mode` field, which will be one of the
[TransferMode](net-http-TransferMode) types.

### Sequence

For example, to send a message of possibly large size:

1. Create the message with a call to `Payload.request` or `Payload.response`.
2. Set the `session` field of the message.
2. Call `Payload.set_length` to indicate the length of the body.
3. Add any additional headers that may be required, such as `Content-type`.
4. Submit the message for transmission by calling the either the
`HTTPSession.apply` method (in servers) or the `HTTPCLient.apply` method
in clients.
5. Wait for the `send_body` notification.
6. Make any number of calls to `Payload.send_chunk`.
7. Call `Payload.finish`.

To send a message of small, reasonable size (say, under 20KB), this
simplified method can be used instead:

1. Create the message with a call to `Payload.request` or `Payload.response`.
2. Set the `session` field of the message.
3. Add any additional headers that may be required, such as `Content-type`.
4. Call `add_chunk` one or more times to add body data.
4. Submit the message for transmission by calling the either the
[HTTPSession](net-http-HTTPSession)`.apply` method (in servers) or the
[HTTPClient](net-http-HTTPClient)`.apply` method in clients.
  """
  var proto: String = "HTTP/1.1"
  var status: U16
  var method: String
  var url: URL
  var _body_length: USize = 0
  var transfer_mode: TransferMode = OneshotTransfer
  var session: (HTTPSession tag | None) = None
  
  embed _headers: Map[String, String] = _headers.create()
  embed _body: Array[ByteSeq val] = _body.create()
  let _response: Bool
  var username: String = ""
  var password: String = ""

  new iso request(method': String = "GET", url': URL = URL) =>
    """
    Create an HTTP `request` message.
    """
    status = 0
    method = method'
    url = url'
    _response = false

  new iso response(status': Status = StatusOK) =>
    """
    Create an HTTP `response` message.
    """
    status = status'()
    method = status'.string()
    url = URL
    _response = true

  new iso _empty(response': Bool = true) =>
    """
    Create an empty HTTP payload.
    """
    status = 0
    method = ""
    url = URL
    _response = response'

  fun apply(key: String): String ? =>
    """
    Get a header.
    """
    _headers(key)

  fun is_safe(): Bool =>
    """
    A request method is "safe" if it does not modify state in the resource.
    These methods can be guaranteed not to have any body data.
    Return true for a safe request method, false otherwise.
    """
    match method
    | "GET"
    | "HEAD"
    | "OPTIONS" =>
      true
    else
      false
    end

  fun body(): this->Array[ByteSeq] ? =>
    """
    Get the body in `OneshotTransfer` mode.
    In the other modes it raises an error.
    """
    match transfer_mode
      | OneshotTransfer => _body
    else
      error
    end

  fun ref set_length(bytecount: (USize | None)) =>
    """
    Set the body length when known in advance.  This determines the
    transfer mode that will be used.  A parameter of 'None' will use
    Chunked Transfer Encoding.  A numeric value will use Streamed
    transfer.  Not calling this function at all will
    use Oneshot transfer.
    """
    match bytecount
      | None  =>
         transfer_mode = ChunkedTransfer
         _headers("Transfer-Encoding") = "chunked"

      | let n: USize =>
         try not _headers.contains("Content-Length") then
           _headers("Content-Length") = n.string()
           end
         _body_length = n
         transfer_mode = StreamTransfer
    end

  fun ref update(key: String, value: String): Payload ref^ =>
    """
    Set any header. If we've already received the header, append the value as a
    comma separated list, as per RFC 2616 section 4.2.
    """
    match _headers(key) = value
    | let prev: String =>
      _headers(key) = prev + "," + value
    end
    this

  fun headers(): this->Map[String, String] =>
    """
    Get all the headers.
    """
    _headers

  fun body_size(): (USize | None) =>
    """
    Get the total intended size of the body.
    `ServerConnection` accumulates actual size transferred for logging.
    """
    match transfer_mode
      | ChunkedTransfer => None
    else
      _body_length
    end

  fun ref add_chunk(data: ByteSeq val): Payload ref^ =>
    """
    This is how application code adds data to the body in
    `OneshotTransfer` mode.  For large bodies, call `set_length`
    and use `send_chunk` instead.
    """
    _body.push(data)
    _body_length = _body_length + data.size()

    this

  fun box send_chunk(data: ByteSeq val) =>
    """
    This is how application code sends body data in `StreamTransfer` and
    `ChunkedTransfer` modes. For smaller body lengths, `add_chunk`
    in `Oneshot` mode can be used instead.
    """
    match session
      | let s: HTTPSession tag =>
        match transfer_mode
          | ChunkedTransfer =>
            // Wrap some body data in the Chunked Transfer Encoding format,
            // which is the length in hex, the data, and a CRLF.  It is
            // important to never send a chunk of length zero, as that is
            // how the end of the body is signalled.
            s.write(Format.int[USize](data.size(), FormatHexBare ))
            s.write("\r\n")
            s.write(data)
            s.write("\r\n")

          | StreamTransfer =>
            // In stream mode just send the data.  Its length should have
            // already been accounted for by `set_length`.
            s.write(data)
          end
        end

  fun val finish() =>
    """
    Mark the end of body transmission.  This does not do anything,
    and is unnecessary, in Oneshot mode.
    """
    match session
      | let s: HTTPSession tag =>
        match transfer_mode
          | ChunkedTransfer =>
             s.write("0\r\n\r\n")
             s.finish()
          | StreamTransfer =>
          s.finish()
        end
      end
       
  fun val respond(response': Payload) =>
    """
    Start sending a response from the server to the client.
    """
    try
      (session as HTTPSession tag)(this)
    end

  fun val _client_fail() =>
    """
    Start sending an error response.
    """
    None
    /* Not sure if we need this.  Nobody calls it.  But something like:
      try
      (session as HTTPSession tag)(
         Payload.response(StatusInternalServerError))
    end */

  fun val _write(keepalive: Bool = true, conn: TCPConnection tag) =>
    """
    Writes the payload to an HTTPSession.  Requests and Responses differ
    only in the first line of text - everything after that is the same
    format.
    """
    if _response then
      _write_response(keepalive, conn)
    else
      _write_request(keepalive, conn)
    end

    _write_common(conn)

  fun val _write_request(keepalive: Bool, conn: TCPConnection tag) =>
    """
    Writes the 'request' parts of an HTTP message.
    """
    conn.write(method + " " + url.path)

    if url.query.size() > 0 then
      conn.write("?" + url.query)
      end

    if url.fragment.size() > 0 then
      conn.write("#" + url.fragment)
      end

    conn.write(" " + proto + "\r\n")

    if not keepalive then
      conn.write("Connection: close\r\n")
      end

    conn.write("Host: " + url.host + ":" + url.port.string() + "\r\n")
  
  fun val _write_common(conn: TCPConnection tag) =>
    """
    Writes the parts of an HTTP message common to both requests and
    responses.
    """
    _write_headers(conn)

    // In oneshot mode we send the entire stored body.
    if transfer_mode is OneshotTransfer then
      for piece in _body.values() do
        conn.write(piece)
      end
    end
          
  fun val _write_response(keepalive: Bool, conn: TCPConnection tag) =>
    """
    Write the response-specific parts of an HTTP message.  This is the
    status line, consisting of the protocol name, the status value,
    and a string representation of the status (carried in the `method`
    field).  Since writing it out is an actor behavior call, we go to
    the trouble of packaging it into a single string before sending.
    """
    let statusline = recover String(
      proto.size() + status.string().size() + method.size() + 4) end

    statusline.append(proto)
    statusline.append(" ")
    statusline.append(status.string())
    statusline.append(" ")
    statusline.append(method)
    statusline.append("\r\n")
    conn.write(consume statusline)

    if keepalive then
      conn.write("Connection: keep-alive\r\n")
    end

  fun _write_headers(conn: TCPConnection tag) =>
    """
    Write all of the HTTP headers to the comm link.
    """
    var saw_length: Bool = false
    for (k, v) in _headers.pairs() do
      if (k != "Host") then
       if k == "Content-Length" then saw_length = true end
       conn.write(k + ": " + v + "\r\n")
       end
     end

     if (not saw_length) and (transfer_mode is OneshotTransfer) then
       conn.write("Content-Length: " + _body_length.string() + "\r\n")
     end

     // Blank line before the body.
     conn.write("\r\n")

  fun box has_body(): Bool =>
    """
    Determines whether a message has a body portion.
    """
    if _response then
      // Errors never have bodies.
      if (status == 204) or // no content
         (status == 304) or // not modified
         ((status > 0) and (status < 200)) or
         (status > 400) then false
      else
        true
      end    
    else
      match transfer_mode
      | ChunkedTransfer => true
      else
        (_body_length > 0)
      end
    end
