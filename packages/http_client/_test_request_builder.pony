use "pony_check"
use "pony_test"

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------

class \nodoc\ iso _PropertyBuilderMethodCorrect
  is Property1[String val]
  """
  Each factory method produces a request with the correct HTTP method.
  """
  fun name(): String => "request_builder/method_correct"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      ["GET"; "HEAD"; "POST"; "PUT"; "DELETE"; "OPTIONS"; "PATCH"])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let req =
      match arg1
      | "GET" => Request.get("/").build()
      | "HEAD" => Request.head("/").build()
      | "POST" => Request.post("/").build()
      | "PUT" => Request.put("/").build()
      | "DELETE" => Request.delete("/").build()
      | "OPTIONS" => Request.options("/").build()
      | "PATCH" => Request.patch("/").build()
      else
      ph.fail("unknown method: " + arg1)
      return
    end
    ph.assert_eq[String val](arg1, req.method.string())

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestBuilderGetBasic is UnitTest
  """Simple GET produces correct method and path."""
  fun name(): String => "request_builder/get_basic"

  fun apply(h: TestHelper) =>
    let req = Request.get("/users").build()
    h.assert_true(req.method is GET, "method should be GET")
    h.assert_eq[String val]("/users", req.path)
    h.assert_true(req.body is None, "GET should have no body")

class \nodoc\ iso _TestBuilderPostWithJSONBody is UnitTest
  """POST with json_body sets body and Content-Type."""
  fun name(): String => "request_builder/post_json_body"

  fun apply(h: TestHelper) =>
    let req = Request.post("/api")
      .json_body("{\"name\": \"Alice\"}")
      .build()
    h.assert_true(req.method is POST, "method should be POST")
    match req.body
    | let b: Array[U8] val =>
      h.assert_eq[String val]("{\"name\": \"Alice\"}", String.from_array(b))
    else
      h.fail("body should be set")
    end
    h.assert_eq[String val](
      "application/json",
      match req.headers.get("Content-Type")
      | let v: String val => v
      else ""
      end)

class \nodoc\ iso _TestBuilderPostWithFormBody is UnitTest
  """POST with form_body encodes params and sets Content-Type."""
  fun name(): String => "request_builder/post_form_body"

  fun apply(h: TestHelper) =>
    let req = Request.post("/login")
      .form_body(recover val [("user", "alice"); ("pass", "s3cret")] end)
      .build()
    match req.body
    | let b: Array[U8] val =>
      h.assert_eq[String val]("user=alice&pass=s3cret", String.from_array(b))
    else
      h.fail("body should be set")
    end
    h.assert_eq[String val](
      "application/x-www-form-urlencoded",
      match req.headers.get("Content-Type")
      | let v: String val => v
      else ""
      end)

class \nodoc\ iso _TestBuilderQueryParams is UnitTest
  """Query params are appended to the path."""
  fun name(): String => "request_builder/query_params"

  fun apply(h: TestHelper) =>
    let req = Request.get("/search")
      .query("q", "hello world")
      .query("page", "1")
      .build()
    h.assert_eq[String val](
      "/search?q=hello%20world&page=1", req.path)

class \nodoc\ iso _TestBuilderHeaders is UnitTest
  """Headers are set on the built request."""
  fun name(): String => "request_builder/headers"

  fun apply(h: TestHelper) =>
    let req = Request.get("/")
      .header("Accept", "application/json")
      .header("X-Custom", "test")
      .build()
    h.assert_eq[String val](
      "application/json",
      match req.headers.get("Accept")
      | let v: String val => v
      else ""
      end)
    h.assert_eq[String val](
      "test",
      match req.headers.get("X-Custom")
      | let v: String val => v
      else ""
      end)

class \nodoc\ iso _TestBuilderBasicAuth is UnitTest
  """basic_auth sets the Authorization header."""
  fun name(): String => "request_builder/basic_auth"

  fun apply(h: TestHelper) =>
    let req = Request.get("/")
      .basic_auth("user", "pass")
      .build()
    let auth_value =
      match req.headers.get("authorization")
      | let v: String val => v
      else ""
      end
    h.assert_true(
      auth_value.contains("Basic "),
      "should have Basic auth header")

class \nodoc\ iso _TestBuilderBearerAuth is UnitTest
  """bearer_auth sets the Authorization header."""
  fun name(): String => "request_builder/bearer_auth"

  fun apply(h: TestHelper) =>
    let req = Request.get("/")
      .bearer_auth("my-token")
      .build()
    h.assert_eq[String val](
      "Bearer my-token",
      match req.headers.get("authorization")
      | let v: String val => v
      else ""
      end)

class \nodoc\ iso _TestBuilderBodyNarrows is UnitTest
  """
  After setting body, build() still works
  (type narrows to RequestOptions).
  """
  fun name(): String => "request_builder/body_narrows"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = [as U8: 1; 2; 3]
    let req = Request.post("/data")
      .header("X-Before", "yes")
      .body(data)
      .header("X-After", "yes")
      .build()
    match req.body
    | let b: Array[U8] val =>
      h.assert_eq[USize](3, b.size())
    else
      h.fail("body should be set")
    end
    // Both headers should be present
    h.assert_eq[String val](
      "yes",
      match req.headers.get("X-Before")
      | let v: String val => v
      else ""
      end)
    h.assert_eq[String val](
      "yes",
      match req.headers.get("X-After")
      | let v: String val => v
      else ""
      end)

class \nodoc\ iso _TestBuilderDeleteWithBody is UnitTest
  """DELETE supports optional body."""
  fun name(): String => "request_builder/delete_with_body"

  fun apply(h: TestHelper) =>
    // DELETE without body
    let req1 = Request.delete("/item/1").build()
    h.assert_true(req1.method is DELETE, "method should be DELETE")
    h.assert_true(req1.body is None, "DELETE without body should have no body")

    // DELETE with body
    let req2 = Request.delete("/item/1")
      .json_body("{\"reason\": \"cleanup\"}")
      .build()
    match \exhaustive\ req2.body
    | let b: Array[U8] val =>
      h.assert_true(b.size() > 0, "DELETE with body should have body")
    else
      h.fail("body should be set")
    end

class \nodoc\ iso _TestBuilderNoQueryParams is UnitTest
  """No query params leaves path unchanged."""
  fun name(): String => "request_builder/no_query_params"

  fun apply(h: TestHelper) =>
    let req = Request.get("/users").build()
    h.assert_eq[String val]("/users", req.path)
