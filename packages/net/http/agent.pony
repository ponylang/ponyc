use "collections"
use "encode/base64"

class iso Agent
  """
  Manages a client request payload.
  """
  let _client: Client
  var _request: Payload

  new iso create(method: String, url: URL, client: Client) =>
    """
    Create an agent for the given HTTP method, URL, and client.
    """
    _client = client
    _request = Payload.request(method, url)

  fun iso headers
  (): HashMap[String val, String val, HashEq[String val] val] box =>
    """
    Get the request headers.
    """
    let t: Agent val = consume this
    recover box t._request.headers() end

  fun iso body(): Array[(String val | Array[U8 val] val)] box =>
    """
    Get the request body.
    """
    let t: Agent val = consume this
    recover box t._request.body() end

  fun iso body_size(): USize =>
    """
    Get the total size of the request body.
    """
    let t: Agent val = consume this
    t._request.body_size()

  fun iso set(key: String, value: String): Agent^ =>
    """
    Set a header field. If we've already received the header, append the value
    as a comma separated list, as per RFC 2616 section 4.2.
    """
    _request.update(key, value)
    consume this

  fun iso basic_auth(username: String, password: String): Agent^ =>
    """
    Set basic authentication header with username and password, as described in
    RFC 2617 section 2.
    """
    let auth = Base64.encode_mime(username + ":" + password)
    _request.update("Authentication", "Basic " + consume auth)
    consume this

  fun iso content_type(content_type': String): Agent^ =>
    """
    Specify the data type to send.
    """
    _request.update("Content-Type", content_type')
    consume this

  fun iso chunk(data: (String val | Array[U8 val] val)): Agent^ =>
    """
    Add chunk to the request body.
    """
    _request.add_chunk(data)
    consume this

  fun iso send(handler: (ResponseHandler | None)) =>
    """
    Send request payload through the client.
    """
    match handler
    | let h: ResponseHandler => _request.handler = h
    end
    let t: Agent val = consume this
    t._client(t._request)
