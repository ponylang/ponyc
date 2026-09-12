class ref _RequestBuilder
  """
  Internal implementation of the request builder.

  Implements all methods from both `RequestOptions` and
  `RequestOptionsWithBody`. The `Request` factory controls which interface
  the caller sees, providing compile-time body restriction for GET/HEAD.
  """
  let _method: Method
  let _path: String
  let _headers: Headers ref
  embed _query_params: Array[(String, String)]
  var _body: (Array[U8] val | None)

  new ref create(method': Method, path': String) =>
    _method = method'
    _path = path'
    _headers = Headers
    _query_params = Array[(String, String)]
    _body = None

  fun ref header(hdr_name: String, hdr_value: String): _RequestBuilder ref =>
    _headers.set(hdr_name, hdr_value)
    this

  fun ref query(key: String, value: String): _RequestBuilder ref =>
    _query_params.push((key, value))
    this

  fun ref basic_auth(username: String, password: String)
    : _RequestBuilder ref
  =>
    (let hdr_name, let hdr_value) = BasicAuth(username, password)
    _headers.set(hdr_name, hdr_value)
    this

  fun ref bearer_auth(token: String): _RequestBuilder ref =>
    (let hdr_name, let hdr_value) = BearerAuth(token)
    _headers.set(hdr_name, hdr_value)
    this

  fun ref body(data: Array[U8] val): _RequestBuilder ref =>
    _body = data
    this

  fun ref json_body(data: String): _RequestBuilder ref =>
    _body = data.array()
    _headers.set("Content-Type", "application/json")
    this

  fun ref form_body(params: Array[(String, String)] val)
    : _RequestBuilder ref
  =>
    _body = FormEncoder(params)
    _headers.set("Content-Type", "application/x-www-form-urlencoded")
    this

  fun ref multipart_body(form: MultipartFormData): _RequestBuilder ref =>
    _body = form.body()
    _headers.set("Content-Type", form.content_type())
    this

  fun ref build(): HTTPRequest val =>
    let full_path = _build_path()
    var hdrs: Headers iso = recover iso Headers end
    for (n, v) in _headers.values() do
      hdrs.set(n, v)
    end
    HTTPRequest(_method, full_path, consume hdrs, _body)

  fun _build_path(): String =>
    if _query_params.size() == 0 then
      _path
    else
      let qsize = _query_params.size()
      var params: Array[(String, String)] iso =
        recover iso Array[(String, String)](qsize) end
      for (k, v) in _query_params.values() do
        params.push((k, v))
      end
      _path + "?" + QueryParams(consume params)
    end
