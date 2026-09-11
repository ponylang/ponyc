use "pony_check"
use "pony_test"

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------

class \nodoc\ iso _PropertyCollectorChunkAccumulation
  is Property1[Array[USize] ref]
  """
  Given N chunks of varying sizes, the built response body has the correct
  total size and the response metadata is preserved.
  """
  fun name(): String => "response_collector/chunk_accumulation"

  fun gen(): Generator[Array[USize] ref] =>
    Generators.array_of[USize](Generators.usize(0, 100) where from = 0)

  fun ref property(arg1: Array[USize] ref, ph: PropertyHelper) =>
    let response = _make_response()
    let collector = ResponseCollector
    collector.set_response(response)

    var expected_size: USize = 0
    var expected_byte: U8 = 0
    for chunk_size in arg1.values() do
      let chunk =
        recover val
          let c = Array[U8](chunk_size)
          var i: USize = 0
          while i < chunk_size do
            c.push(expected_byte)
            i = i + 1
          end
          c
        end
      expected_byte = expected_byte + 1
      expected_size = expected_size + chunk_size
      collector.add_chunk(chunk)
    end

    try
      let result = collector.build()?
      ph.assert_eq[USize](expected_size, result.body.size())
      ph.assert_eq[U16](200, result.status)
    else
      ph.fail("build() should not error after set_response()")
    end

  fun _make_response(): Response val =>
    let headers = recover val Headers end
    Response(HTTP11, 200, "OK", headers)

class \nodoc\ iso _PropertyCollectorPreservesResponseMetadata
  is Property1[U16]
  """
  The built HTTPResponse preserves the status code from the original Response.
  """
  fun name(): String => "response_collector/preserves_metadata"

  fun gen(): Generator[U16] =>
    Generators.u16(100, 599)

  fun ref property(arg1: U16, ph: PropertyHelper) =>
    let response = Response(HTTP11, arg1, "Test", recover val Headers end)
    let collector = ResponseCollector
    collector.set_response(response)

    try
      let result = collector.build()?
      ph.assert_eq[U16](arg1, result.status)
      ph.assert_eq[String val]("Test", result.reason)
    else
      ph.fail("build() should not error after set_response()")
    end

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestCollectorBuildCorrectness is UnitTest
  """Built response has correct version, status, reason, headers, and body."""
  fun name(): String => "response_collector/build_correctness"

  fun apply(h: TestHelper) =>
    let hdrs =
      recover val
        Headers .> set("Content-Type", "text/plain")
      end
    let response = Response(HTTP10, 201, "Created", hdrs)
    let collector = ResponseCollector
    collector.set_response(response)
    collector.add_chunk([as U8: 'H'; 'e'; 'l'; 'l'; 'o'])
    collector.add_chunk([as U8: ' '; 'W'; 'o'; 'r'; 'l'; 'd'])

    try
      let result = collector.build()?
      h.assert_true(result.version is HTTP10, "version should be HTTP/1.0")
      h.assert_eq[U16](201, result.status)
      h.assert_eq[String val]("Created", result.reason)
      h.assert_eq[String val](
        "text/plain",
        match result.headers.get("Content-Type")
        | let v: String val => v
        else ""
        end)
      h.assert_eq[USize](11, result.body.size())
      h.assert_eq[String val](
        "Hello World",
        String.from_array(result.body))
    else
      h.fail("build() should not error after set_response()")
    end

class \nodoc\ iso _TestCollectorEmptyBody is UnitTest
  """No chunks produces an empty body."""
  fun name(): String => "response_collector/empty_body"

  fun apply(h: TestHelper) =>
    let response =
      Response(
        HTTP11, 204, "No Content", recover val Headers end)
    let collector = ResponseCollector
    collector.set_response(response)

    try
      let result = collector.build()?
      h.assert_eq[USize](0, result.body.size())
      h.assert_eq[U16](204, result.status)
    else
      h.fail("build() should not error after set_response()")
    end

class \nodoc\ iso _TestCollectorSingleChunk is UnitTest
  """Single chunk is preserved exactly."""
  fun name(): String => "response_collector/single_chunk"

  fun apply(h: TestHelper) =>
    let response = Response(HTTP11, 200, "OK", recover val Headers end)
    let collector = ResponseCollector
    collector.set_response(response)
    let data: Array[U8] val = [as U8: 1; 2; 3; 4; 5]
    collector.add_chunk(data)

    try
      let result = collector.build()?
      h.assert_eq[USize](5, result.body.size())
      h.assert_eq[U8](1, try result.body(0)? else 0 end)
      h.assert_eq[U8](5, try result.body(4)? else 0 end)
    else
      h.fail("build() should not error after set_response()")
    end

class \nodoc\ iso _TestCollectorBuildWithoutResponse is UnitTest
  """build() errors when set_response() was never called."""
  fun name(): String => "response_collector/build_without_response"

  fun apply(h: TestHelper) =>
    let collector = ResponseCollector
    collector.add_chunk([as U8: 1; 2; 3])

    try
      collector.build()?
      h.fail("build() should error without set_response()")
    end
