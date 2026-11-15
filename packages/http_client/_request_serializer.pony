primitive _RequestSerializer
  """
  Serialize an `HTTPRequest` into HTTP/1.1 wire format.

  Produces: `METHOD SP PATH SP HTTP/1.1\r\n` + headers + `\r\n` + body.

  Auto-sets `Host` from the connection's host/port if not already present.
  Port is omitted when it is the default for the connection's scheme (80
  for http, 443 for https) or empty; included otherwise (RFC 9110 §7.2).
  Auto-sets `Content-Length` from body size if body is present and neither
  `Content-Length` nor `Transfer-Encoding` is already in the request headers
  (RFC 9112 section 6.2). User-explicit headers take precedence.
  """

  fun apply(
    request: HTTPRequest val,
    host: String,
    port: String,
    secure: Bool)
    : Array[U8] iso^
  =>
    let buf = recover iso Array[U8] end

    // Request line: METHOD SP PATH SP HTTP/1.1\r\n
    buf .> append(request.method.string())
      .> push(' ')
      .> append(request.path)
      .> append(" HTTP/1.1\r\n")

    // Host header (auto-set if not present).
    // IPv6 addresses are bracketed per RFC 9110 section 7.2.
    let default_port: String = if secure then "443" else "80" end
    if request.headers.get("host") is None then
      buf.append("Host: ")
      if host.contains(":") then
        buf .> push('[')
          .> append(host)
          .> push(']')
      else
        buf.append(host)
      end
      if (port != default_port) and (port != "") then
        buf .> push(':')
          .> append(port)
      end
      buf.append("\r\n")
    end

    // Content-Length header (auto-set if body present and neither
    // Content-Length nor Transfer-Encoding is already set)
    match request.body
    | let b: Array[U8] val =>
      if
        (request.headers.get("content-length") is None) and
        (request.headers.get("transfer-encoding") is None)
      then
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
