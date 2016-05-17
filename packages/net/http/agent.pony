use "collections"
use "encode/base64"

class Agent
  """
  Manages a client request payload.
  """
  let _method: String
  let _url: URL

  let _headers: Map[String, String] = _headers.create()
  let _body: Array[ByteSeq] = _body.create()

  new get(url: URL) =>
    """
    Create an Agent for a GET request.
    """
    _method = "GET"
    _url = url

  new post(url: URL) =>
    """
    Create an Agent for a POST request.
    """
    _method = "POST"
    _url = url

  new put(url: URL) =>
    """
    Create an Agent for a PUT request.
    """
    _method = "PUT"
    _url = url

  new head(url: URL) =>
    """
    Create an Agent for a HEAD request.
    """
    _method = "HEAD"
    _url = url

  new delete(url: URL) =>
    """
    Create an Agent for a DELETE request.
    """
    _method = "DELETE"
    _url = url

  new patch(url: URL) =>
    """
    Create an Agent for a PATCH request.
    """
    _method = "PATCH"
    _url = url

  new options(url: URL) =>
    """
    Create an Agent for an OPTIONS request.
    """
    _method = "OPTIONS"
    _url = url

  new custom_method(method: String, url: URL) =>
    """
    Create an Agent for a request using a custom HTTP method.
    """
    _method = method
    _url = url

  fun headers(): Map[String, String] box =>
    """
    Get the request headers.
    """
    _headers

  fun body(): Array[ByteSeq] box =>
    """
    Get the request body.
    """
    body()

  fun body_size(): USize =>
    """
    Get the total size of the request body.
    """
    var len = USize(0)
    for v in _body.values() do
      len = len + v.size()
    end
    len

  fun ref set(key: String, value: String): Agent =>
    """
    Set a header field. If we've already received the header, append the value
    as a comma separated list, as per RFC 2616 section 4.2.
    """
    match _headers(key) = value
    | let prev: String =>
      _headers(key) = prev + "," + value
    end
    this

  fun ref basic_auth(username: String, password: String): Agent =>
    """
    Set basic authentication header with username and password, as described in
    RFC 2617 section 2.
    """
    let auth = Base64.encode_mime(username + ":" + password)
    _headers.update("Authentication", "Basic " + consume auth)
    this

  fun ref content_type(content_type': String): Agent =>
    """
    Specify the data type to send.
    """
    _headers.update("Content-Type", content_type')
    this

  fun ref chunk(data: (String val | Array[U8 val] val)): Agent =>
    """
    Add chunk to the request body.
    """
    _body.push(data)
    this

  fun ref send(client: Client, handler: (ResponseHandler | None)) =>
    """
    Send request payload through the client.
    """
    let req = Payload.request(_method, _url, handler)
    for (k, v) in _headers.pairs() do
      req.update(k, v)
    end
    for bs in _body.values() do
      req.add_chunk(bs)
    end
    client(consume req)
