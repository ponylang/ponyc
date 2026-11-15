use "format"
use "pony_test"

// ---------------------------------------------------------------------------
// Test helper: recording implementation of _ResponseParserNotify
// ---------------------------------------------------------------------------

class \nodoc\ _TestResponseParserNotify is _ResponseParserNotify
  embed responses: Array[(U16, String val, Version, Headers val)] =
    Array[(U16, String val, Version, Headers val)]
  embed body_chunks: Array[Array[U8] val] = Array[Array[U8] val]
  var completed: USize = 0
  embed errors: Array[ParseError] = Array[ParseError]

  fun ref response_received(
    status: U16,
    reason: String val,
    version: Version,
    headers: Headers val)
  =>
    responses.push((status, reason, version, headers))

  fun ref body_chunk(data: Array[U8] val) =>
    body_chunks.push(data)

  fun ref response_complete() =>
    completed = completed + 1

  fun ref parse_error(err: ParseError) =>
    errors.push(err)

  fun ref collected_body_string(): String val =>
    """
    Concatenate all body chunks into a single string.
    """
    let out = String
    for chunk in body_chunks.values() do
      out.append(chunk)
    end
    out.clone()

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _PropertyValidStatusLineParsesCorrectly
  is Property[(U16, String val)]
  """
  Valid (status, reason) pairs serialized as HTTP/1.1 status lines parse
  correctly, delivering response_received with matching values and then
  response_complete.
  """
  fun name(): String => "parser/valid_status_line"

  fun gen(): Generator[(U16, String val)] =>
    Generators.zip2[U16, String val](
      Generators.u16(200, 599),
      Generators.one_of[String val](
        [ "OK"; "Not Found"; "Internal Server Error"
          "Created"; "Moved Permanently"; "" ]))

  fun ref property(arg1: (U16, String val), ph: PropertyHelper) =>
    (let status, let reason) = arg1
    let raw: String val =
      "HTTP/1.1 " + status.string() + " " + reason +
      "\r\nContent-Length: 0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    ph.assert_eq[USize](
      1, notify.completed, "should have 1 completion")
    ph.assert_eq[USize](
      0, notify.errors.size(), "should have 0 errors")
    try
      (let s, let r, let v, _) = notify.responses(0)?
      ph.assert_eq[U16](status, s, "status mismatch")
      ph.assert_eq[String val](reason, r, "reason mismatch")
      ph.assert_true(v is HTTP11, "version should be HTTP/1.1")
    else
      ph.fail("could not read response")
    end

class \nodoc\ iso _PropertyInvalidStatusLineRejected
  is Property[String val]
  """
  Malformed status lines produce parse errors.
  """
  fun name(): String => "parser/invalid_status_line_rejected"

  fun gen(): Generator[String val] =>
    Generators.frequency[String val](
      [ as WeightedGenerator[String val]:
        // No space after version
        (1, Generators.unit[String val]("HTTP/1.1200 OK\r\n\r\n"))
        // Non-numeric status
        (1, Generators.unit[String val]("HTTP/1.1 abc OK\r\n\r\n"))
        // Too short
        (1, Generators.unit[String val]("HTTP/1.1\r\n\r\n"))
        // Two-digit status
        (1, Generators.unit[String val]("HTTP/1.1 20 OK\r\n\r\n"))
        // Random garbage
        (1, Generators.ascii(5, 30)
          .map[String val]({(s) => s + "\r\n\r\n" }))
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover arg1.array().clone() end)

    ph.assert_eq[USize](
      0,
      notify.responses.size(),
      "should have 0 responses for: " + arg1)
    ph.assert_eq[USize](
      1,
      notify.errors.size(),
      "should have 1 error for: " + arg1)

class \nodoc\ iso _PropertyHeadersRoundtrip
  is Property[Array[(String val, String val)] ref]
  """
  Headers in a response are correctly parsed and available in the
  delivered Headers collection.
  """
  fun name(): String => "parser/headers_roundtrip"

  fun gen(): Generator[Array[(String val, String val)] ref] =>
    let pair_gen =
      Generators.zip2[String val, String val](
        Generators.ascii_letters(1, 10),
        Generators.ascii_letters(1, 20))
    Generators.array_of[
      (String val, String val)](pair_gen, 1, 5)

  fun ref property(
    arg1: Array[(String val, String val)] ref,
    ph: PropertyHelper)
  =>
    var raw: String val = "HTTP/1.1 200 OK\r\n"
    raw = raw + "Content-Length: 0\r\n"
    for (hdr_name, hdr_value) in arg1.values() do
      raw = raw + hdr_name + ": " + hdr_value + "\r\n"
    end
    raw = raw + "\r\n"

    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    try
      (_, _, _, let headers) = notify.responses(0)?
      let seen = Array[String val]
      for (hdr_name, hdr_value) in arg1.values() do
        let lower: String val = hdr_name.lower()
        // Skip "content-length" since we added it ourselves
        if lower == "content-length" then continue end
        var already_seen = false
        for s in seen.values() do
          if s == lower then already_seen = true; break end
        end
        if not already_seen then
          seen.push(lower)
          match \exhaustive\ headers.get(hdr_name)
          | let v: String val =>
            ph.assert_eq[String val](
              hdr_value,
              v,
              "header " + hdr_name + " mismatch")
          | None =>
            ph.fail("header " + hdr_name + " not found")
          end
        end
      end
    else
      ph.fail("could not read response")
    end

class \nodoc\ iso _PropertyFixedBodyDelivered
  is Property[USize]
  """
  Responses with Content-Length have their body delivered completely via
  body_chunk callbacks, followed by response_complete.
  """
  fun name(): String => "parser/fixed_body_delivered"

  fun gen(): Generator[USize] =>
    Generators.usize(1, 200)

  fun ref property(arg1: USize, ph: PropertyHelper) =>
    let body =
      recover val
        let b = Array[U8](arg1)
        var i: USize = 0
        while i < arg1 do
          b.push('A' + (i % 26).u8())
          i = i + 1
        end
        b
      end

    let raw =
      recover val
        let r = String
          .> append("HTTP/1.1 200 OK\r\n")
          .> append("Content-Length: " + arg1.string() + "\r\n")
          .> append("\r\n")
          .> append(body)
        r
      end

    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    ph.assert_eq[USize](
      1, notify.completed, "should have 1 completion")
    ph.assert_eq[USize](
      0, notify.errors.size(), "should have 0 errors")

    var total_body: USize = 0
    for chunk in notify.body_chunks.values() do
      total_body = total_body + chunk.size()
    end
    ph.assert_eq[USize](arg1, total_body, "body size mismatch")

    var byte_idx: USize = 0
    for chunk in notify.body_chunks.values() do
      try
        var ci: USize = 0
        while ci < chunk.size() do
          ph.assert_eq[U8](
            body(byte_idx)?,
            chunk(ci)?,
            "body byte mismatch at " + byte_idx.string())
          byte_idx = byte_idx + 1
          ci = ci + 1
        end
      end
    end

class \nodoc\ iso _PropertyChunkedBodyDelivered
  is Property[Array[USize] ref]
  """
  Chunked transfer encoding delivers the complete body and
  response_complete.
  """
  fun name(): String => "parser/chunked_body_delivered"

  fun gen(): Generator[Array[USize] ref] =>
    Generators.array_of[USize](Generators.usize(1, 50), 1, 5)

  fun ref property(arg1: Array[USize] ref, ph: PropertyHelper) =>
    var total_size: USize = 0
    var raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n" +
      "\r\n"

    for size in arg1.values() do
      let chunk_data: String val =
        recover val
          let s = String(size)
          var i: USize = 0
          while i < size do
            s.push('A' + (i % 26).u8())
            i = i + 1
          end
          s
        end
      let hex_size: String val =
        Format.int[USize](size where fmt = FormatHexBare)
      raw = raw + hex_size + "\r\n" + chunk_data + "\r\n"
      total_size = total_size + size
    end
    raw = raw + "0\r\n\r\n"

    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    ph.assert_eq[USize](
      1, notify.completed, "should have 1 completion")
    ph.assert_eq[USize](
      0, notify.errors.size(), "should have 0 errors")

    var total_received: USize = 0
    for chunk in notify.body_chunks.values() do
      total_received = total_received + chunk.size()
    end
    ph.assert_eq[USize](
      total_size,
      total_received,
      "total body size mismatch")

class \nodoc\ iso _PropertyStatusLineBoundary
  is Property[(String val, Bool)]
  """
  Mixed valid/invalid status lines: valid ones produce response_received,
  invalid ones produce parse_error.
  """
  fun name(): String => "parser/status_line_boundary"

  fun gen(): Generator[(String val, Bool)] =>
    let valid_gen: Generator[(String val, Bool)] =
      Generators.one_of[String val](
        [ "200"; "201"; "204"; "301"; "302"; "304"
          "400"; "401"; "403"; "404"; "500"; "502"
          "503"; "599" ])
        .map[(String val, Bool)]({(status) =>
          ("HTTP/1.1 " + status +
            " OK\r\nContent-Length: 0\r\n\r\n", true)
        })

    let invalid_gen: Generator[(String val, Bool)] =
      Generators.frequency[String val](
        [ as WeightedGenerator[String val]:
          (1, Generators.unit[String val](
            "HTTP/2.0 200 OK\r\n\r\n"))
          (1, Generators.unit[String val](
            "HTTP/1.1 abc OK\r\n\r\n"))
          (1, Generators.unit[String val](
            "GARBAGE\r\n\r\n"))
          (1, Generators.unit[String val](
            "HTTP/1.1\r\n\r\n"))
        ]).map[(String val, Bool)](
          {(s) => (s, false) })

    Generators.frequency[(String val, Bool)](
      [ as WeightedGenerator[(String val, Bool)]:
        (1, valid_gen)
        (1, invalid_gen)
      ])

  fun ref property(arg1: (String val, Bool), ph: PropertyHelper) =>
    (let raw, let should_succeed) = arg1
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    if should_succeed then
      ph.assert_eq[USize](
        1,
        notify.responses.size(),
        "valid response should parse")
      ph.assert_eq[USize](
        0,
        notify.errors.size(),
        "valid response should have no errors")
    else
      ph.assert_eq[USize](
        1,
        notify.errors.size(),
        "invalid response should produce error")
    end

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestParserKnownGoodResponses is UnitTest
  """
  Verify exact callback sequences for well-known HTTP responses.
  """
  fun name(): String => "parser/known_good_responses"

  fun apply(h: TestHelper) =>
    _test_simple_200(h)
    _test_404_with_body(h)
    _test_301_redirect(h)

  fun _test_simple_200(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 5\r\n" +
      "\r\n" +
      "Hello"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "200: 1 response")
    h.assert_eq[USize](1, notify.completed, "200: 1 completion")
    try
      (let s, let r, let v, _) = notify.responses(0)?
      h.assert_eq[U16](200, s, "status should be 200")
      h.assert_eq[String val]("OK", r, "reason should be OK")
      h.assert_true(v is HTTP11, "version should be HTTP/1.1")
    else
      h.fail("200: could not read response")
    end
    h.assert_eq[String val](
      "Hello", notify.collected_body_string(), "body should be Hello")

  fun _test_404_with_body(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 404 Not Found\r\n" +
      "Content-Length: 9\r\n" +
      "\r\n" +
      "Not Found"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "404: 1 response")
    h.assert_eq[USize](1, notify.completed, "404: 1 completion")
    try
      (let s, let r, _, _) = notify.responses(0)?
      h.assert_eq[U16](404, s, "status should be 404")
      h.assert_eq[String val]("Not Found", r)
    else
      h.fail("404: could not read response")
    end
    h.assert_eq[String val](
      "Not Found", notify.collected_body_string())

  fun _test_301_redirect(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 301 Moved Permanently\r\n" +
      "Location: https://example.com/new\r\n" +
      "Content-Length: 0\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "301: 1 response")
    h.assert_eq[USize](1, notify.completed, "301: 1 completion")
    try
      (let s, _, _, let hdrs) = notify.responses(0)?
      h.assert_eq[U16](301, s)
      h.assert_eq[String val](
        "https://example.com/new",
        match \exhaustive\ hdrs.get("Location")
        | let v: String val => v
        else "" end)
    else
      h.fail("301: could not read response")
    end

class \nodoc\ iso _TestIncrementalByteByByte is UnitTest
  """
  Feed a complete response one byte at a time. Verify the parser produces
  the same result as feeding all at once.
  """
  fun name(): String => "parser/incremental_byte_by_byte"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 5\r\n" +
      "\r\n" +
      "Hello"

    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)

    var i: USize = 0
    let arr = raw.array()
    while i < arr.size() do
      try
        let byte =
          recover iso
            let a = Array[U8](1)
              .> push(arr(i)?)
            a
          end
        parser.parse(consume byte)
      end
      i = i + 1
    end

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.errors.size(), "0 errors")
    h.assert_eq[String val](
      "Hello", notify.collected_body_string(), "body should be Hello")

class \nodoc\ iso _TestSizeLimitStatusLine is UnitTest
  """Status line exceeding small limit -> TooLarge."""
  fun name(): String => "parser/size_limit_status_line"

  fun apply(h: TestHelper) =>
    let config = _ParserConfig(where max_status_line_size' = 16)
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify, config)

    let raw = "HTTP/1.1 200 OK with a long reason\r\n\r\n"
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(notify.errors(0)? is TooLarge, "should be TooLarge")
    end

class \nodoc\ iso _TestSizeLimitHeaders is UnitTest
  """Headers exceeding small limit -> TooLarge."""
  fun name(): String => "parser/size_limit_headers"

  fun apply(h: TestHelper) =>
    let config = _ParserConfig(where max_header_size' = 32)
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify, config)

    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "X-Very-Long-Header-Name: very-long-value-that-exceeds-limit\r\n" +
      "\r\n"
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(notify.errors(0)? is TooLarge, "should be TooLarge")
    end

class \nodoc\ iso _TestSizeLimitBody is UnitTest
  """Content-Length or chunked body exceeding limit -> BodyTooLarge."""
  fun name(): String => "parser/size_limit_body"

  fun apply(h: TestHelper) =>
    let config = _ParserConfig(where max_body_size' = 5)

    // Fixed body: Content-Length 10 > max_body_size 5
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify, config)
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 10\r\n\r\n" +
      "0123456789"
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1,
      notify.errors.size(),
      "fixed body: should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is BodyTooLarge,
        "fixed body: should be BodyTooLarge")
    end

    // Chunked body: 10 bytes > max_body_size 5
    let notify2: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser2 = _ResponseParser(notify2, config)
    let raw2: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n" +
      "a\r\n0123456789\r\n0\r\n\r\n"
    parser2.parse(recover raw2.array().clone() end)

    h.assert_eq[USize](
      1,
      notify2.errors.size(),
      "chunked body: should have 1 error")
    try
      h.assert_true(
        notify2.errors(0)? is BodyTooLarge,
        "chunked body: should be BodyTooLarge")
    end

class \nodoc\ iso _TestSizeLimitCloseDelimitedBody is UnitTest
  """Close-delimited body exceeding limit -> BodyTooLarge."""
  fun name(): String => "parser/size_limit_close_delimited_body"

  fun apply(h: TestHelper) =>
    let config = _ParserConfig(where max_body_size' = 5)
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify, config)

    // Response with no CL/chunked => close-delimited
    let raw: String val =
      "HTTP/1.1 200 OK\r\n\r\n" +
      "0123456789"  // 10 bytes > max 5
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is BodyTooLarge,
        "should be BodyTooLarge")
    end

class \nodoc\ iso _TestInvalidContentLength is UnitTest
  """Non-numeric Content-Length -> InvalidContentLength."""
  fun name(): String => "parser/invalid_content_length"

  fun apply(h: TestHelper) =>
    let raw = "HTTP/1.1 200 OK\r\nContent-Length: abc\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is InvalidContentLength,
        "should be InvalidContentLength")
    end

class \nodoc\ iso _TestInvalidChunkSize is UnitTest
  """Non-hex chunk size -> InvalidChunk."""
  fun name(): String => "parser/invalid_chunk_size"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n" +
      "xyz\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is InvalidChunk,
        "should be InvalidChunk")
    end

class \nodoc\ iso _TestChunkSizeNonHexRejected is UnitTest
  """Chunk sizes with non-hex characters are rejected per RFC 9112 §7.1."""
  fun name(): String => "parser/chunk_size_non_hex_rejected"

  fun apply(h: TestHelper) =>
    let head: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n"
    let nul = recover val String .> push(0) .> append("10") end
    let high = recover val String .> push(0x80) .> append("10") end
    let sizes: Array[String val] val =
      ["1_0"; "_10"; "10_"; "0_"; "_0"; "_"; nul; high]
    for size in sizes.values() do
      let raw: String val =
        head + size + "\r\nabcdefghijklmnop\r\n0\r\n\r\n"
      let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
      let parser = _ResponseParser(notify)
      parser.parse(recover raw.array().clone() end)

      h.assert_true(
        notify.errors.size() > 0,
        "size '" + size + "' should produce an error")
      try
        h.assert_true(
          notify.errors(0)? is InvalidChunk,
          "size '" + size + "' should be InvalidChunk")
      end
    end

class \nodoc\ iso _TestChunkSizeEmptyBeforeExtRejected is UnitTest
  """Empty chunk size before a chunk extension is rejected."""
  fun name(): String => "parser/chunk_size_empty_before_ext_rejected"

  fun apply(h: TestHelper) =>
    let head: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n"
    // ";ext" has no size at all; " ;ext" and "\t;ext" have only BWS
    // before the semicolon, so BWS stripping produces an empty size.
    let sizes: Array[String val] val = [";ext"; " ;ext"; "\t;ext"]
    for size in sizes.values() do
      let raw: String val =
        head + size + "\r\nabcdefghijklmnop\r\n0\r\n\r\n"
      let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
      let parser = _ResponseParser(notify)
      parser.parse(recover raw.array().clone() end)

      h.assert_true(
        notify.errors.size() > 0,
        "size '" + size + "' should produce an error")
      try
        h.assert_true(
          notify.errors(0)? is InvalidChunk,
          "size '" + size + "' should be InvalidChunk")
      end
    end

class \nodoc\ iso _TestChunkSizeBWSBeforeSemicolon is UnitTest
  """BWS (SP/HTAB) before the chunk extension semicolon is accepted."""
  fun name(): String => "parser/chunk_size_bws_before_semicolon"

  fun apply(h: TestHelper) =>
    let head: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n"
    let sizes: Array[String val] val = ["5 ;ext"; "5\t;ext"; "5 \t;ext"]
    for size in sizes.values() do
      let raw: String val =
        head + size + "\r\nHello\r\n0\r\n\r\n"
      let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
      let parser = _ResponseParser(notify)
      parser.parse(recover raw.array().clone() end)

      h.assert_eq[USize](
        1, notify.responses.size(), "size '" + size + "': 1 response")
      h.assert_eq[USize](
        1, notify.completed, "size '" + size + "': 1 completion")
      h.assert_eq[USize](
        0, notify.errors.size(), "size '" + size + "': 0 errors")
      h.assert_eq[String val](
        "Hello",
        notify.collected_body_string(),
        "size '" + size + "': body mismatch")
    end

class \nodoc\ iso _TestChunkSizeUppercaseHex is UnitTest
  """Uppercase hex digits in chunk sizes are accepted."""
  fun name(): String => "parser/chunk_size_uppercase_hex"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n" +
      "A\r\nHelloWorld\r\n0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.errors.size(), "0 errors")
    h.assert_eq[String val](
      "HelloWorld", notify.collected_body_string())

class \nodoc\ iso _TestChunkSizeTrailingSpaceRejected is UnitTest
  """Trailing whitespace without a semicolon is rejected."""
  fun name(): String => "parser/chunk_size_trailing_space_rejected"

  fun apply(h: TestHelper) =>
    let head: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n"
    let sizes: Array[String val] val = ["5 "; "5\t"; "5 \t"]
    for size in sizes.values() do
      let raw: String val =
        head + size + "\r\nHello\r\n0\r\n\r\n"
      let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
      let parser = _ResponseParser(notify)
      parser.parse(recover raw.array().clone() end)

      h.assert_true(
        notify.errors.size() > 0,
        "size '" + size + "' should produce an error")
      try
        h.assert_true(
          notify.errors(0)? is InvalidChunk,
          "size '" + size + "' should be InvalidChunk")
      end
    end

class \nodoc\ iso _TestChunkedWithTrailers is UnitTest
  """Chunked response with trailer headers -> trailers skipped, completes."""
  fun name(): String => "parser/chunked_with_trailers"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n\r\n" +
      "5\r\nHello\r\n" +
      "0\r\n" +
      "Trailer-Header: value\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.errors.size(), "0 errors")
    h.assert_eq[String val](
      "Hello", notify.collected_body_string())

class \nodoc\ iso _TestHTTP10Version is UnitTest
  """HTTP/1.0 response -> version is HTTP10."""
  fun name(): String => "parser/http10_version"

  fun apply(h: TestHelper) =>
    let raw = "HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      h.assert_true(
        notify.responses(0)?._3 is HTTP10,
        "version should be HTTP/1.0")
    end

class \nodoc\ iso _TestInvalidVersion is UnitTest
  """Bad version string -> InvalidVersion."""
  fun name(): String => "parser/invalid_version"

  fun apply(h: TestHelper) =>
    _test_version(h, "HTTP/2.0")
    _test_version(h, "HTTP/1.2")
    _test_version(h, "HTXP/1.1")
    _test_version(h, "HTTP/1")

  fun _test_version(h: TestHelper, ver: String val) =>
    let raw: String val = ver + " 200 OK\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)
    h.assert_eq[USize](
      1,
      notify.errors.size(),
      "should have 1 error for " + ver)
    try
      let err = notify.errors(0)?
      h.assert_true(
        (err is InvalidVersion) or (err is InvalidStatusLine),
        "should be InvalidVersion or InvalidStatusLine for " + ver)
    end

class \nodoc\ iso _TestNoBodyHead is UnitTest
  """HEAD response with Content-Length -> body suppressed."""
  fun name(): String => "parser/no_body_head"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 1000\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.expect_response(HEAD)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.body_chunks.size(), "0 body chunks")

class \nodoc\ iso _TestNoBody204 is UnitTest
  """204 No Content -> no body regardless of headers."""
  fun name(): String => "parser/no_body_204"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 204 No Content\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.body_chunks.size(), "0 body chunks")

class \nodoc\ iso _TestNoBody304 is UnitTest
  """304 Not Modified -> no body regardless of headers."""
  fun name(): String => "parser/no_body_304"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 304 Not Modified\r\n" +
      "Content-Length: 500\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.body_chunks.size(), "0 body chunks")

class \nodoc\ iso _TestCloseDelimitedBody is UnitTest
  """
  Response with no CL/chunked: body delivered via chunks, completed by
  connection_closed().
  """
  fun name(): String => "parser/close_delimited_body"

  fun apply(h: TestHelper) =>
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)

    // First chunk of data
    let raw: String val = "HTTP/1.1 200 OK\r\n\r\nHello"
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](
      0,
      notify.completed,
      "not completed yet (close-delimited)")

    // More data
    parser.parse(recover iso " World".array().clone() end)

    // Connection close signals end of body
    parser.connection_closed()

    h.assert_eq[USize](1, notify.completed, "completed after close")
    h.assert_eq[String val](
      "Hello World", notify.collected_body_string())

class \nodoc\ iso _TestContentLengthZero is UnitTest
  """Content-Length: 0 -> response_complete immediately (no body)."""
  fun name(): String => "parser/content_length_zero"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 0\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.body_chunks.size(), "0 body chunks")

class \nodoc\ iso _TestContentLengthAndChunked is UnitTest
  """
  Both Content-Length and Transfer-Encoding: chunked -> chunked takes
  precedence per RFC 9112 §6.3.
  """
  fun name(): String => "parser/content_length_and_chunked"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 100\r\n" +
      "Transfer-Encoding: chunked\r\n" +
      "\r\n" +
      "5\r\nHello\r\n0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size(), "1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    h.assert_eq[USize](0, notify.errors.size(), "0 errors")
    // Body is 5 bytes (chunked), not 100 (Content-Length)
    h.assert_eq[String val](
      "Hello", notify.collected_body_string())

class \nodoc\ iso _TestDuplicateContentLength is UnitTest
  """Two Content-Length headers with differing values -> error."""
  fun name(): String => "parser/duplicate_content_length"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 10\r\n" +
      "Content-Length: 20\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.errors.size(), "should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is InvalidContentLength,
        "should be InvalidContentLength")
    end

class \nodoc\ iso _TestDataAfterError is UnitTest
  """
  Feed data after a parse error -> no additional callbacks fired
  (verifies _failed short-circuit).
  """
  fun name(): String => "parser/data_after_error"

  fun apply(h: TestHelper) =>
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)

    // Cause an error
    let bad = "GARBAGE_NOT_HTTP\r\n\r\n"
    parser.parse(recover bad.array().clone() end)
    h.assert_eq[USize](1, notify.errors.size(), "1 error after bad data")

    // Feed valid data after error
    let good = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
    parser.parse(recover good.array().clone() end)

    // Should still have exactly 1 error and 0 responses
    h.assert_eq[USize](
      1,
      notify.errors.size(),
      "still 1 error after second parse")
    h.assert_eq[USize](
      0,
      notify.responses.size(),
      "0 responses (parser is failed)")

class \nodoc\ iso _Test1xxSkipped is UnitTest
  """100 Continue followed by 200 OK: only 200 delivered to handler."""
  fun name(): String => "parser/1xx_skipped"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 100 Continue\r\n" +
      "\r\n" +
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 2\r\n" +
      "\r\n" +
      "OK"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    // Only the 200 should be delivered
    h.assert_eq[USize](
      1,
      notify.responses.size(),
      "should have 1 response (not 2)")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    try
      h.assert_eq[U16](
        200,
        notify.responses(0)?._1,
        "delivered response should be 200")
    end
    h.assert_eq[String val](
      "OK", notify.collected_body_string())

class \nodoc\ iso _TestMultiple1xx is UnitTest
  """Multiple 1xx responses before final response."""
  fun name(): String => "parser/multiple_1xx"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 100 Continue\r\n\r\n" +
      "HTTP/1.1 102 Processing\r\n\r\n" +
      "HTTP/1.1 200 OK\r\n" +
      "Content-Length: 4\r\n" +
      "\r\n" +
      "Done"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "should have 1 response")
    h.assert_eq[USize](1, notify.completed, "1 completion")
    try
      h.assert_eq[U16](200, notify.responses(0)?._1)
    end
    h.assert_eq[String val](
      "Done", notify.collected_body_string())

class \nodoc\ iso _TestHeaderValueBareLFSanitized is UnitTest
  """A bare LF in a header value is replaced with SP per RFC 9110 §5.5."""
  fun name(): String => "parser/header_value_bare_lf_sanitized"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "X-Test: before\nafter\r\n" +
      "Content-Length: 0\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      let hdrs = notify.responses(0)?._4
      match \exhaustive\ hdrs.get("x-test")
      | let v: String =>
        h.assert_eq[String val]("before after", v)
      | None => h.fail("x-test header missing")
      end
    end

class \nodoc\ iso _TestHeaderValueBareCRSanitized is UnitTest
  """A bare CR in a header value is replaced with SP per RFC 9110 §5.5."""
  fun name(): String => "parser/header_value_bare_cr_sanitized"

  fun apply(h: TestHelper) =>
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "X-Test: before\rafter\r\n" +
      "Content-Length: 0\r\n" +
      "\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      let hdrs = notify.responses(0)?._4
      match \exhaustive\ hdrs.get("x-test")
      | let v: String =>
        h.assert_eq[String val]("before after", v)
      | None => h.fail("x-test header missing")
      end
    end

class \nodoc\ iso _TestHeaderValueNulSanitized is UnitTest
  """A NUL byte in a header value is replaced with SP per RFC 9110 §5.5."""
  fun name(): String => "parser/header_value_nul_sanitized"

  fun apply(h: TestHelper) =>
    let raw =
      recover val
        String
          .> append("HTTP/1.1 200 OK\r\n")
          .> append("X-Test: before")
          .> push(0)
          .> append("after\r\n")
          .> append("Content-Length: 0\r\n")
          .> append("\r\n")
      end
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      let hdrs = notify.responses(0)?._4
      match \exhaustive\ hdrs.get("x-test")
      | let v: String =>
        h.assert_eq[String val]("before after", v)
      | None => h.fail("x-test header missing")
      end
    end

class \nodoc\ iso _TestHeaderValueMultipleControlCharsSanitized is UnitTest
  """Multiple control characters in one header value are all replaced."""
  fun name(): String => "parser/header_value_multiple_control_chars_sanitized"

  fun apply(h: TestHelper) =>
    let raw =
      recover val
        String
          .> append("HTTP/1.1 200 OK\r\n")
          .> append("X-Test: a")
          .> push('\n')
          .> append("b")
          .> push('\r')
          .> append("c")
          .> push(0)
          .> append("d\r\n")
          .> append("Content-Length: 0\r\n")
          .> append("\r\n")
      end
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      let hdrs = notify.responses(0)?._4
      match \exhaustive\ hdrs.get("x-test")
      | let v: String =>
        h.assert_eq[String val]("a b c d", v)
      | None => h.fail("x-test header missing")
      end
    end

class \nodoc\ iso _TestHeaderValueLocationLFSanitized is UnitTest
  """
  A Location header with a bare LF in the path — the vector for redirect
  header injection (#6544) — has the LF replaced with SP by the parser.
  """
  fun name(): String => "parser/header_value_location_lf_sanitized"

  fun apply(h: TestHelper) =>
    let raw =
      recover val
        String
          .> append("HTTP/1.1 302 Found\r\n")
          .> append("Location: /next HTTP/1.1\nX-Injected: 1\nX-Z:\r\n")
          .> append("Content-Length: 0\r\n")
          .> append("\r\n")
      end
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      let hdrs = notify.responses(0)?._4
      match \exhaustive\ hdrs.get("location")
      | let v: String =>
        h.assert_false(
          v.contains("\n"), "Location must not contain bare LF after parsing")
        h.assert_false(
          v.contains("\r"), "Location must not contain bare CR after parsing")
        h.assert_eq[String val](
          "/next HTTP/1.1 X-Injected: 1 X-Z:", v)
      | None => h.fail("location header missing")
      end
    end

class \nodoc\ iso _TestTransferEncodingParsing is UnitTest
  """
  Transfer-Encoding header values are parsed as a comma-separated list of
  codings. The body is chunked only when the final coding is "chunked"
  (case-insensitive) on an HTTP/1.1 response. All other cases use
  close-delimited framing.
  """
  fun name(): String => "parser/transfer_encoding_parsing"

  fun apply(h: TestHelper) =>
    _test_substring_not_matched(h, "notchunked")
    _test_substring_not_matched(h, "xchunkedx")
    _test_chunked_not_final(h)
    _test_chunked_final_in_list(h)
    _test_case_insensitive(h)
    _test_two_te_lines_last_wins(h)
    _test_two_te_lines_last_chunked(h)
    _test_http10_with_te_chunked(h)
    _test_te_non_chunked_ignores_content_length(h)

  fun _test_substring_not_matched(h: TestHelper, te_value: String val) =>
    """
    A value that contains "chunked" as a substring but is not the token
    "chunked" must not trigger chunked framing.
    """
    let body_bytes = "3\r\nabc\r\n0\r\n\r\n"
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: " + te_value + "\r\n" +
      "\r\n" +
      body_bytes
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), te_value + ": 1 response")
    h.assert_eq[USize](
      0, notify.completed, te_value + ": not completed (close-delimited)")
    h.assert_eq[USize](
      0, notify.errors.size(), te_value + ": 0 errors")

    parser.connection_closed()
    h.assert_eq[USize](
      1, notify.completed, te_value + ": completed after close")
    h.assert_eq[String val](
      body_bytes,
      notify.collected_body_string(),
      te_value + ": raw body (not chunk-decoded)")

  fun _test_chunked_not_final(h: TestHelper) =>
    """
    "chunked, gzip" — chunked is not the final coding, so body is
    close-delimited per RFC 9112 §6.3 item 4.
    """
    let body_bytes = "3\r\nabc\r\n0\r\n\r\n"
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked, gzip\r\n" +
      "\r\n" +
      body_bytes
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "chunked,gzip: 1 response")
    h.assert_eq[USize](
      0, notify.completed, "chunked,gzip: not completed (close-delimited)")

    parser.connection_closed()
    h.assert_eq[USize](
      1, notify.completed, "chunked,gzip: completed after close")
    h.assert_eq[String val](
      body_bytes,
      notify.collected_body_string(),
      "chunked,gzip: raw body")

  fun _test_chunked_final_in_list(h: TestHelper) =>
    """
    "gzip, chunked" — chunked is the final coding, so chunked framing applies.
    """
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: gzip, chunked\r\n" +
      "\r\n" +
      "3\r\nabc\r\n0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "gzip,chunked: 1 response")
    h.assert_eq[USize](
      1, notify.completed, "gzip,chunked: completed (chunked)")
    h.assert_eq[USize](
      0, notify.errors.size(), "gzip,chunked: 0 errors")
    h.assert_eq[String val](
      "abc",
      notify.collected_body_string(),
      "gzip,chunked: decoded body")

  fun _test_case_insensitive(h: TestHelper) =>
    """
    "CHUNKED" — transfer coding names are case-insensitive per RFC 9110.
    """
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: CHUNKED\r\n" +
      "\r\n" +
      "3\r\nabc\r\n0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "CHUNKED: 1 response")
    h.assert_eq[USize](
      1, notify.completed, "CHUNKED: completed (chunked)")
    h.assert_eq[USize](
      0, notify.errors.size(), "CHUNKED: 0 errors")
    h.assert_eq[String val](
      "abc",
      notify.collected_body_string(),
      "CHUNKED: decoded body")

  fun _test_two_te_lines_last_wins(h: TestHelper) =>
    """
    Two Transfer-Encoding headers: "chunked" then "gzip". The effective list
    is "chunked, gzip", so the final coding is gzip — close-delimited.
    """
    let body_bytes = "3\r\nabc\r\n0\r\n\r\n"
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n" +
      "Transfer-Encoding: gzip\r\n" +
      "\r\n" +
      body_bytes
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "two lines: 1 response")
    h.assert_eq[USize](
      0, notify.completed, "two lines: not completed (close-delimited)")

    parser.connection_closed()
    h.assert_eq[USize](
      1, notify.completed, "two lines: completed after close")
    h.assert_eq[String val](
      body_bytes,
      notify.collected_body_string(),
      "two lines: raw body")

  fun _test_two_te_lines_last_chunked(h: TestHelper) =>
    """
    Two Transfer-Encoding headers: "gzip" then "chunked". The effective list
    is "gzip, chunked", so the final coding is chunked — chunked framing.
    """
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: gzip\r\n" +
      "Transfer-Encoding: chunked\r\n" +
      "\r\n" +
      "3\r\nabc\r\n0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "two lines chunked: 1 response")
    h.assert_eq[USize](
      1, notify.completed, "two lines chunked: completed (chunked)")
    h.assert_eq[USize](
      0, notify.errors.size(), "two lines chunked: 0 errors")
    h.assert_eq[String val](
      "abc",
      notify.collected_body_string(),
      "two lines chunked: decoded body")

  fun _test_http10_with_te_chunked(h: TestHelper) =>
    """
    HTTP/1.0 with Transfer-Encoding: chunked — RFC 9112 §6.1 says treat
    the framing as faulty. The body is read until close.
    """
    let body_bytes = "3\r\nabc\r\n0\r\n\r\n"
    let raw: String val =
      "HTTP/1.0 200 OK\r\n" +
      "Transfer-Encoding: chunked\r\n" +
      "\r\n" +
      body_bytes
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "HTTP/1.0 TE: 1 response")
    h.assert_eq[USize](
      0, notify.completed, "HTTP/1.0 TE: not completed (close-delimited)")

    parser.connection_closed()
    h.assert_eq[USize](
      1, notify.completed, "HTTP/1.0 TE: completed after close")
    h.assert_eq[String val](
      body_bytes,
      notify.collected_body_string(),
      "HTTP/1.0 TE: raw body (not chunk-decoded)")

  fun _test_te_non_chunked_ignores_content_length(h: TestHelper) =>
    """
    Transfer-Encoding present but not chunked, with Content-Length also
    present — Content-Length is ignored per RFC 9112 §6.3 item 3.
    """
    let raw: String val =
      "HTTP/1.1 200 OK\r\n" +
      "Transfer-Encoding: gzip\r\n" +
      "Content-Length: 100\r\n" +
      "\r\n" +
      "some data"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "TE+CL: 1 response")
    h.assert_eq[USize](
      0, notify.completed, "TE+CL: not completed (close-delimited, not fixed)")
    h.assert_eq[USize](
      0, notify.errors.size(), "TE+CL: 0 errors")

    parser.connection_closed()
    h.assert_eq[USize](
      1, notify.completed, "TE+CL: completed after close")
    h.assert_eq[String val](
      "some data",
      notify.collected_body_string(),
      "TE+CL: body is close-delimited data")

class \nodoc\ iso _TestHeaderNameBareLFSanitized is UnitTest
  """A bare LF in a header name is replaced with SP."""
  fun name(): String => "parser/header_name_bare_lf_sanitized"

  fun apply(h: TestHelper) =>
    let raw =
      recover val
        String
          .> append("HTTP/1.1 200 OK\r\n")
          .> append("X")
          .> push('\n')
          .> append("Test: hello\r\n")
          .> append("Content-Length: 0\r\n")
          .> append("\r\n")
      end
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      let hdrs = notify.responses(0)?._4
      match \exhaustive\ hdrs.get("x test")
      | let v: String =>
        h.assert_eq[String val]("hello", v)
      | None => h.fail("x test header missing after name sanitization")
      end
    end

class \nodoc\ iso _TestReasonPhraseBareLFSanitized is UnitTest
  """A bare LF in the reason phrase is replaced with SP."""
  fun name(): String => "parser/reason_phrase_bare_lf_sanitized"

  fun apply(h: TestHelper) =>
    let raw =
      recover val
        String
          .> append("HTTP/1.1 200 O")
          .> push('\n')
          .> append("K\r\n")
          .> append("Content-Length: 0\r\n")
          .> append("\r\n")
      end
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](1, notify.responses.size())
    try
      let reason = notify.responses(0)?._2
      h.assert_false(
        reason.contains("\n"), "Reason must not contain bare LF after parsing")
      h.assert_eq[String val]("O K", reason)
    end

class \nodoc\ iso _TestInvalidStatusLineExtraDigits is UnitTest
  """Extra characters after the three-digit status code are rejected."""
  fun name(): String => "parser/invalid_status_line_extra_digits"

  fun apply(h: TestHelper) =>
    _test_rejected(
      h,
      "HTTP/1.1 2000 OK\r\nContent-Length: 0\r\n\r\n",
      "fourth digit before space")
    _test_rejected(
      h,
      "HTTP/1.1 200OK\r\nContent-Length: 0\r\n\r\n",
      "letter instead of space")
    _test_rejected(
      h,
      "HTTP/1.1 20000\r\nContent-Length: 0\r\n\r\n",
      "fourth and fifth digits, no space")
    _test_rejected(
      h,
      "HTTP/1.1 1000 x\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n",
      "1xx with fourth digit")
    _test_rejected(
      h,
      "HTTP/1.1 200\tOK\r\nContent-Length: 0\r\n\r\n",
      "tab instead of space")
    _test_empty_reason_valid(h)

  fun _test_rejected(h: TestHelper, raw: String val, case: String val) =>
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      0, notify.responses.size(), case + ": should have 0 responses")
    h.assert_eq[USize](
      1, notify.errors.size(), case + ": should have 1 error")
    try
      h.assert_true(
        notify.errors(0)? is InvalidStatusLine,
        case + ": should be InvalidStatusLine")
    end

  fun _test_empty_reason_valid(h: TestHelper) =>
    """Status code at end of line with no trailing space is valid."""
    let raw = "HTTP/1.1 200\r\nContent-Length: 0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    h.assert_eq[USize](
      1, notify.responses.size(), "empty reason: should have 1 response")
    h.assert_eq[USize](
      0, notify.errors.size(), "empty reason: should have 0 errors")
    try
      (let s, let r, _, _) = notify.responses(0)?
      h.assert_eq[U16](200, s, "empty reason: status should be 200")
      h.assert_eq[String val](
        "", r, "empty reason: reason should be empty")
    else
      h.fail("empty reason: could not read response")
    end

class \nodoc\ iso _PropertyExtraDigitsAfterStatusRejected
  is Property[U16]
  """A fourth digit after the status code produces InvalidStatusLine."""
  fun name(): String => "parser/extra_digit_after_status_rejected"

  fun gen(): Generator[U16] =>
    Generators.u16(100, 599)

  fun ref property(arg1: U16, ph: PropertyHelper) =>
    let extra_digit: String val = (arg1 % 10).string()
    let raw: String val =
      "HTTP/1.1 " + arg1.string() + extra_digit +
      " OK\r\nContent-Length: 0\r\n\r\n"
    let notify: _TestResponseParserNotify ref = _TestResponseParserNotify
    let parser = _ResponseParser(notify)
    parser.parse(recover raw.array().clone() end)

    ph.assert_eq[USize](
      0, notify.responses.size(), "should have 0 responses for: " + raw)
    ph.assert_eq[USize](
      1, notify.errors.size(), "should have 1 error for: " + raw)
    try
      ph.assert_true(
        notify.errors(0)? is InvalidStatusLine,
        "should be InvalidStatusLine for: " + raw)
    end

