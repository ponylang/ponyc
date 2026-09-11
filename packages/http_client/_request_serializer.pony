primitive _RequestSerializer
  """
  Serialize an `HTTPRequest` into HTTP/1.1 wire format.

  Produces: `METHOD SP PATH SP HTTP/1.1\r\n` + headers + `\r\n` + body.

  Auto-sets `Host` from the connection's host/port if not already present.
  Port is omitted for "80" and "443" (standard ports); included otherwise.
  Auto-sets `Content-Length` from body size if body is present and not already
  in the request headers. User-explicit headers take precedence.
  """

  fun apply(
    request: HTTPRequest val,
    host: String,
    port: String)
    : Array[U8] iso^
  =>
    let buf = recover iso Array[U8] end

    // Request line: METHOD SP PATH SP HTTP/1.1\r\n
    buf .> append(request.method.string())
      .> push(' ')
      .> append(request.path)
      .> append(" HTTP/1.1\r\n")

    // Host header (auto-set if not present)
    if request.headers.get("host") is None then
      buf.append("Host: ")
      buf.append(host)
      if (port != "80") and (port != "443") and (port != "") then
        buf .> push(':')
          .> append(port)
      end
      buf.append("\r\n")
    end

    // Content-Length header (auto-set if body present and not already set)
    match request.body
    | let b: Array[U8] val =>
      if request.headers.get("content-length") is None then
        buf .> append("Content-Length: ")
          .> append(b.size().string())
          .> append("\r\n")
      end
    end

    // User headers
    for (name, value) in request.headers.values() do
      buf .> append(name)
        .> append(": ")
        .> append(value)
        .> append("\r\n")
    end

    // End of headers
    buf.append("\r\n")

    // Body
    match request.body
    | let b: Array[U8] val => buf.append(b)
    end

    buf
