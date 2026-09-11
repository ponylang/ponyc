## Add uri package to the standard library

The `uri` package from [ponylang/uri](https://github.com/ponylang/uri) is now part of the standard library. It provides RFC 3986 URI parsing, building, normalization, resolution, and equivalence checking, plus RFC 6570 URI template expansion and `application/x-www-form-urlencoded` parsing.

```pony
use "uri"

actor Main
  new create(env: Env) =>
    match ParseURI("https://example.com/path?key=value#frag")
    | let u: URI => env.out.print(u.string())
    | let e: URIParseError => env.err.print(e.string())
    end
```

The `uri/template` subpackage handles RFC 6570 URI template expansion:

```pony
use "uri/template"

actor Main
  new create(env: Env) =>
    try
      let tpl = URITemplate("{+user}/inbox")?
      let vars = URITemplateVariables.>set("user", "fred")
      env.out.print(tpl.expand(vars))
    end
```

## Add http_client package to the standard library

The `courier` package from [ponylang/courier](https://github.com/ponylang/courier), renamed to `http_client`, is now part of the standard library. It provides an HTTP/1.1 client with streaming callbacks, request building, response collection, redirect following, multipart form uploads, and typed JSON decoding.

```pony
use "http_client"
use "net"

actor MyClient is HTTPClientConnectionActor
  var _http: HTTPClientConnection = HTTPClientConnection.none()
  let _out: OutStream

  new create(auth: TCPConnectAuth, host: String, port: String,
    out: OutStream)
  =>
    _out = out
    _http = HTTPClientConnection(auth, host, port, this,
      ClientConnectionConfig)

  fun ref _http_client_connection(): HTTPClientConnection => _http

  fun ref on_connected() =>
    _http.send_request(HTTPRequest(GET, "/"))

  fun ref on_response(response: Response val) =>
    _out.print(response.status.string() + " " + response.reason)

  fun ref on_body_chunk(data: Array[U8] val) =>
    _out.write(data)

  fun ref on_response_complete() =>
    _out.print("")
    _http.close()
```
