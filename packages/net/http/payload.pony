use "collections"
use "encode/base64"
use "net"

class iso Payload
  """
  An HTTP payload. For a response, the method indicates the status text. For a
  request, the status is meaningless.
  """
  var handler: (ResponseHandler | None)
  var proto: String = "HTTP/1.1"
  var status: U16
  var method: String
  var url: URL

  let _headers: Map[String, String] = _headers.create()
  let _body: Array[ByteSeq] = _body.create()
  let _response: Bool

  new iso request(method': String = "GET", url': URL = URL,
    handler': (ResponseHandler | None) = None)
  =>
    """
    Create an HTTP request.
    """
    handler = handler'
    status = 0
    method = method'
    url = url'
    _response = false

  new iso response(status': U16 = 200, description: String = "OK",
    handler': (ResponseHandler | None) = None)
  =>
    """
    Create an HTTP response.
    """
    handler = handler'
    status = status'
    method = description
    url = URL
    _response = true

  new iso _empty(response': Bool = true) =>
    """
    Create an empty HTTP payload.
    """
    handler = None
    status = 0
    method = ""
    url = URL
    _response = response'

  fun apply(key: String): String ? =>
    """
    Get a header.
    """
    _headers(key)

  fun ref update(key: String, value: String): Payload ref^ =>
    """
    Set a header. If we've already received the header, append the value as a
    comma separated list, as per RFC 2616 section 4.2.
    """
    match _headers(key) = value
    | let prev: String =>
      _headers(key) = prev + "," + value
    end
    this

  fun headers(): this->Map[String, String] =>
    """
    Get the headers.
    """
    _headers

  fun body(): this->Array[ByteSeq] =>
    """
    Get the body.
    """
    _body

  fun body_size(): USize =>
    """
    Get the total size of the body.
    """
    var len = USize(0)

    for v in _body.values() do
      len = len + v.size()
    end

    len

  fun ref add_chunk(data: ByteSeq): Payload ref^ =>
    """
    Add a chunk to the body.
    """
    _body.push(data)
    this

  fun ref media_type(content_type': String): Payload ref^ =>
    """
    Specify the media type to send.
    """
    _headers.update("Content-Type", content_type')
    this

  fun ref basic_auth(username: String, password: String): Payload ref^ =>
    """
    Set basic authentication header with username and password, as described in
    RFC 2617 section 2.
    """
    let auth = Base64.encode_mime(username + ":" + password)
    _headers.update("Authentication", "Basic " + consume auth)
    this

  fun iso respond(response': Payload) =>
    """
    Trigger the response handler.
    """
    try
      let h = (handler = None) as ResponseHandler
      h(consume this, consume response')
    end

  fun iso fail() =>
    """
    Trigger the response handler with an error payload.
    """
    try
      let h = (handler = None) as ResponseHandler
      h(consume this, Payload.response(0))
    end

  fun val _client_respond(response': Payload) =>
    """
    Trigger the response handler. This is private to prevent request handlers
    from responding to a request more than once.
    """
    try
      let h = handler as ResponseHandler
      h(consume this, consume response')
    end

  fun val _client_fail() =>
    """
    Trigger the response handler with an error payload. This is private to
    prevent request handlers from responding to a request more than once.
    """
    try
      let h = handler as ResponseHandler
      h(consume this, Payload.response(0))
    end

  fun _write(conn: TCPConnection, keepalive: Bool = true) =>
    """
    Writes the payload to a TCP connection.
    """
    if _response then
      _write_response(conn, keepalive)
    else
      _write_request(conn, keepalive)
    end

  fun _write_request(conn: TCPConnection, keepalive: Bool) =>
    """
    Writes an an HTTP request.
    """
    let len = 15 + (4 * _headers.size()) + (4 * _body.size())
    var list = recover Array[ByteSeq](len) end

    list.push(method)
    list.push(" ")
    list.push(url.path)

    if url.query.size() > 0 then
      list.push("?")
      list.push(url.query)
    end

    if url.fragment.size() > 0 then
      list.push("#")
      list.push(url.fragment)
    end

    list.push(" ")
    list.push(proto)

    if not keepalive then
      list.push("\r\nConnection: close")
    end

    list.push("\r\nHost: ")
    list.push(url.host)
    list.push(":")
    list.push(url.port.string())

    list = _add_headers(consume list)
    list = _add_body(consume list)

    conn.writev(consume list)

  fun _write_response(conn: TCPConnection, keepalive: Bool) =>
    """
    Write as an HTTP response.
    """
    let len = 8 + (4 * _headers.size()) + (4 * _body.size())
    var list = recover Array[ByteSeq](len) end

    list.push(proto)
    list.push(" ")
    list.push(status.string())
    list.push(" ")
    list.push(method)

    if keepalive then
      list.push("\r\nConnection: keep-alive")
    end

    list = _add_headers(consume list)
    list = _add_body(consume list)

    conn.writev(consume list)

  fun _add_headers(list: Array[ByteSeq] iso): Array[ByteSeq] iso^ =>
    """
    Add the headers to the list.
    """
    for (k, v) in _headers.pairs() do
      if
        (k != "Host") and
        (k != "Content-Length")
      then
        list.push("\r\n")
        list.push(k)
        list.push(": ")
        list.push(v)
      end
    end

    list

  fun _add_body(list: Array[ByteSeq] iso): Array[ByteSeq] iso^ =>
    """
    Add the body to the list.
    TODO: don't include the body for HEAD, 204, 304 or 1xx
    """
    if _body.size() > 0 then
      list.push("\r\nContent-Length: ")
      list.push(body_size().string())
      list.push("\r\n\r\n")

      for v in _body.values() do
        list.push(v)
      end
    else
      list.push("\r\nContent-Length: 0\r\n\r\n")
    end

    list

  fun val string(): String =>
    let str = recover String end
    match _response
    | true =>
      str.append(proto)
      str.append(" ")
      str.append(status.string())
      str.append(" ")
      str.append(StatusMessage(status))
      str.append("\n")
    else // request
      str.append(method)
      str.append(" ")
      str.append(proto)
      str.append("\n")
    end

    for (k, v) in headers().pairs() do
      str.append(k)
      str.append(": ")
      str.append(v)
      str.append("\n")
    end
    if headers().size() > 0 then str.append("\n") end

    for chunk in body().values() do
      str.append(match chunk
      | let s: String val => s
      | let bs: Array[U8 val] val => String.from_array(bs)
      else
        ""
      end)
    end
    if body().size() > 0 then str.append("\n") end
    str

primitive Method
  fun get(): String => "GET"
  fun post(): String => "POST"
  fun put(): String => "PUT"
  fun head(): String => "HEAD"
  fun delete(): String => "DELETE"
  fun patch(): String => "PATCH"
  fun options(): String => "OPTIONS"

primitive MediaType
  fun html(): String => "text/html"
  fun json(): String => "application/json"
  fun xml(): String => "application/xml"
  fun text(): String => "text/plain"
  fun urlencoded(): String => "application/x-www-form-urlencoded"

primitive StatusMessage
  fun apply(status: U16): String =>
    match status
    | 100 => "Continue"
    | 101 => "Switching Protocols"
    | 200 => "OK"
    | 201 => "Created"
    | 202 => "Accepted"
    | 203 => "Non-Authoritative Information"
    | 204 => "No Content"
    | 205 => "Reset Content"
    | 206 => "Partial Content"
    | 300 => "Multiple Choices"
    | 301 => "Moved Permanently"
    | 302 => "Found"
    | 303 => "See Other"
    | 304 => "Not Modified"
    | 305 => "Use Proxy"
    | 306 => "Temporary Redirect"
    | 400 => "Bad Request"
    | 401 => "Unauthorized"
    | 402 => "Payment Required"
    | 403 => "Forbidden"
    | 404 => "Not Found"
    | 405 => "Method Not Allowed"
    | 406 => "Not Acceptable"
    | 407 => "Proxy Authentication Required"
    | 408 => "Request Timeout"
    | 409 => "Conflict"
    | 410 => "Gone"
    | 411 => "Length Required"
    | 412 => "Precondition Failed"
    | 413 => "Request Entity Too Large"
    | 414 => "Request URI Too Long"
    | 415 => "Unsupported Media Type"
    | 416 => "Requested Range Not Satisfiable"
    | 417 => "Expectation Failed"
    | 418 => "I'm a teapot"
    | 428 => "Precondition Required"
    | 429 => "Too Many Requests"
    | 431 => "Request Header Fields Too Large"
    | 451 => "Unavailable For Legal Reasons"
    | 500 => "Internal Server Error"
    | 501 => "Not Implemented"
    | 502 => "Bad Gateway"
    | 503 => "Service Unavailable"
    | 504 => "Gateway Timeout"
    | 505 => "HTTP Version Not Supported"
    | 511 => "Network Authentication Required"
    else
      ""
    end
