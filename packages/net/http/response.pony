use "collections"

class Response val
  """
  An HTTP response object.
  """
  var _proto: String = ""
  var _status: U16 = 0
  var _status_text: String = ""
  let _headers: Map[String, String] = _headers.create()
  var _body: Array[Array[U8] val] = _body.create()

  new iso create() =>
    """
    Create a new response, which will be filled in as data is parsed.
    """
    None

  fun proto(): String =>
    """
    Get the response protocol.
    """
    _proto

  fun ref set_proto(to: String): Response ref^ =>
    """
    Set the response protocol.
    """
    _proto = to
    this

  fun status(): U16 =>
    """
    Get the status code.
    """
    _status

  fun ref set_status(code: U16): Response ref^ =>
    """
    Set the status code.
    """
    _status = code
    this

  fun status_text(): String =>
    """
    Get the status text.
    """
    _status_text

  fun ref set_status_text(text: String): Response ref^ =>
    """
    Set the status text.
    """
    _status_text = text
    this

  fun headers(): this->Map[String, String] =>
    """
    Get the headers.
    """
    _headers

  fun apply(key: String): String ? =>
    """
    Get a header.
    """
    _headers(key)

  fun ref update(key: String, value: String): Response ref^ =>
    """
    Set a header. If we've already received the header, append the value as a
    comma separated list, as per RFC 2616 section 4.2.
    """
    match _headers(key) = value
    | let prev: String =>
      _headers(key) = prev + "," + value
    end
    this

  fun body(): this->Array[Array[U8] val] =>
    """
    Get the body.
    """
    _body

  fun ref add_chunk(data: Array[U8] val): Response ref^ =>
    """
    Add a chunk to the body.
    """
    _body.push(data)
    this
