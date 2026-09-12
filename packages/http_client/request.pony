primitive Request
  """
  Factory for building HTTP requests with a typed step-builder pattern.

  Each factory method returns a builder typed according to whether the HTTP
  method supports a body:

  - `get()` and `head()` return `RequestOptions` — no body methods available.
  - `post()`, `put()`, `patch()`, `delete()`, and `options()` return
    `RequestOptionsWithBody` — body methods are available, but optional.

  After calling a body method, the return type narrows to `RequestOptions`,
  preventing the body from being set twice.

  CONNECT and TRACE are intentionally omitted. CONNECT is for proxy tunneling
  (not standard request/response) and TRACE is a diagnostic verb rarely used
  by application code. Both exist as `Method` primitives and can be used
  directly: `HTTPRequest(CONNECT, path)` / `HTTPRequest(TRACE, path)`.

  ```pony
  // Simple GET
  let req = Request.get("/users").build()

  // GET with query params and auth
  let req = Request.get("/search")
    .query("q", "pony lang")
    .bearer_auth("my-token")
    .build()

  // POST with JSON body
  let req = Request.post("/users")
    .json_body("{\"name\": \"Alice\"}")
    .build()

  // POST with form body
  let req = Request.post("/login")
    .form_body(recover val [("user", "alice"); ("pass", "secret")] end)
    .build()

  // POST with multipart form data
  let form = MultipartFormData
    .> field("username", "alice")
    .> file("avatar", "photo.jpg", "image/jpeg", image_data)
  let req = Request.post("/upload")
    .multipart_body(form)
    .build()
  ```
  """

  fun get(path: String): RequestOptions ref^ =>
    """
    Create a GET request builder.
    """
    _RequestBuilder(GET, path)

  fun head(path: String): RequestOptions ref^ =>
    """
    Create a HEAD request builder.
    """
    _RequestBuilder(HEAD, path)

  fun delete(path: String): RequestOptionsWithBody ref^ =>
    """
    Create a DELETE request builder.
    """
    _RequestBuilder(DELETE, path)

  fun options(path: String): RequestOptionsWithBody ref^ =>
    """
    Create an OPTIONS request builder.
    """
    _RequestBuilder(OPTIONS, path)

  fun post(path: String): RequestOptionsWithBody ref^ =>
    """
    Create a POST request builder.
    """
    _RequestBuilder(POST, path)

  fun put(path: String): RequestOptionsWithBody ref^ =>
    """
    Create a PUT request builder.
    """
    _RequestBuilder(PUT, path)

  fun patch(path: String): RequestOptionsWithBody ref^ =>
    """
    Create a PATCH request builder.
    """
    _RequestBuilder(PATCH, path)
