use "http_client"
use "files"
use json = "json"
use "net"

class val Todo
  """
  A todo item from jsonplaceholder.typicode.com.
  """
  let title: String
  let completed: Bool

  new val create(title': String, completed': Bool) =>
    title = title'
    completed = completed'

primitive TodoDecoder is JSONDecoder[Todo]
  """
  Decode a JSON object into a `Todo`.
  """
  fun apply(value: json.JSONValue): (Todo | JSONDecodeError) =>
    let nav = json.JSONNav(value)
    try
      Todo(nav("title").as_string()?, nav("completed").as_bool()?)
    else
      JSONDecodeError(
        "expected object with string 'title' and boolean 'completed'")
    end

actor Main
  new create(env: Env) =>
    let auth = TCPConnectAuth(env.root)
    try
      let ssl_ctx =
        recover val
          SSLContext
            .> set_client_verify(true)
            .> set_authority(
              FilePath(
                FileAuth(env.root),
                "/etc/ssl/certs/ca-certificates.crt"))?
        end
      JSONAPIClient(auth, ssl_ctx, env.out)
    else
      env.out.print("Failed to initialize SSL context")
    end

actor JSONAPIClient is HTTPClientConnectionActor
  """
  JSON API client that fetches a JSON endpoint over HTTPS and parses it.

  Usage: ./json-api
  Connects to jsonplaceholder.typicode.com over HTTPS, fetches a todo item,
  decodes the JSON response into a typed `Todo` object, and prints selected
  fields.

  Demonstrates `Request` builder for request construction, `ResponseCollector`
  for body accumulation, `JSONDecoder` and `DecodeJSON` for typed JSON
  decoding, and `HTTPClientConnection.ssl()` for TLS connections.
  """
  var _http: HTTPClientConnection = HTTPClientConnection.none()
  var _collector: ResponseCollector = ResponseCollector
  let _out: OutStream

  new create(
    auth: TCPConnectAuth,
    ssl_ctx: SSLContext val,
    out: OutStream)
  =>
    _out = out
    _http =
      HTTPClientConnection.ssl(
        auth,
        ssl_ctx,
        "jsonplaceholder.typicode.com",
        "443",
        this,
        ClientConnectionConfig)

  fun ref _http_client_connection(): HTTPClientConnection => _http

  fun ref on_connected() =>
    let req = Request.get("/todos/1")
      .header("Accept", "application/json")
      .build()
    _http.send_request(req)

  fun ref on_connection_failure(reason: ConnectionFailureReason) =>
    _out.print("Connection failed")

  fun ref on_response(response: Response val) =>
    _collector = ResponseCollector
    _collector.set_response(response)
    _out.print("< " + response.status.string() + " " + response.reason)

  fun ref on_body_chunk(data: Array[U8] val) =>
    _collector.add_chunk(data)

  fun ref on_response_complete() =>
    try
      let response = _collector.build()?
      match \exhaustive\ DecodeJSON[Todo](response, TodoDecoder)
      | let todo: Todo =>
        _out.print("Todo item:")
        _out.print("  title: " + todo.title)
        _out.print("  completed: " + todo.completed.string())
      | let err: json.JSONParseError =>
        _out.print("JSON parse error: " + err.string())
      | let err: JSONDecodeError =>
        _out.print("JSON decode error: " + err.string())
      end
    end
    _http.close()

  fun ref on_closed() =>
    _out.print("Connection closed")
