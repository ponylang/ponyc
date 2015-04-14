use "collections"
use "net"

class Request val
  """
  An HTTP request object.
  """
  let _handler: HTTPNotify
  let _method: String
  let _url: URL
  let _headers: Map[String, String] = Map[String, String]
  var _body: Bytes = ""

  new iso create(method: String, url: URL, handler: HTTPNotify) =>
    """
    Creates a request for a given method and resource.
    """
    _handler = handler
    _method = method
    _url = url

  fun method(): String =>
    """
    Get the request method.
    """
    _method

  fun url(): URL =>
    """
    Get the requested url.
    """
    _url

  fun apply(key: String ): String ? =>
    """
    Retrieve a header.
    """
    _headers(key)

  fun ref update(key: String, value: String): Request ref^ =>
    """
    Set a header.
    """
    _headers(key) = value
    this

  fun body(): Bytes =>
    """
    Retrieve the body.
    """
    _body

  fun ref set_body(value: Bytes): Request ref^ =>
    """
    Set the body.
    """
    _body = value
    this

  fun _write(host: String, service: String, conn: TCPConnection) =>
    """
    Write the request on the supplied TCP connection.
    """
    let len = 13 + (4 * _headers.size())
    let list = recover Array[Bytes](len) end

    list.push(_method)
    list.push(" ")
    list.push(_url.path)
    list.push(" HTTP/1.1\r\n")
    list.push("Host: ")
    list.push(host)
    list.push(":")
    list.push(service)

    try
      for (k, v) in _headers.pairs() do
        list.push("\r\n")
        list.push(k)
        list.push(": ")
        list.push(v)
      end
    end

    if _body.size() > 0 then
      list.push("\r\n")
      list.push("Content-Length: ")
      list.push(_body.size().string())
    end

    list.push("\r\n\r\n")

    if _body.size() > 0 then
      list.push(_body)
    end

    conn.writev(consume list)

  fun val _response(response: Response) =>
    """
    Notify the handler that we have a response.
    """
    _handler(this, response)
