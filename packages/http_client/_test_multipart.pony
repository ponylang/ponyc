use "pony_check"
use "pony_test"

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------

class \nodoc\ iso _PropertyMultipartBodyStructure
  is Property1[USize]
  """
  For a form with N parts, the serialized body contains exactly N+1
  boundary occurrences (N part boundaries + 1 closing boundary).
  """
  fun name(): String => "multipart/body_structure"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 10)

  fun ref property(arg1: USize, ph: PropertyHelper) =>
    let form = MultipartFormData
    var i: USize = 0
    while i < arg1 do
      if (i % 2) == 0 then
        form.field("field" + i.string(), "value" + i.string())
      else
        let data: Array[U8] val =
          recover val
            [as U8: 0xDE; 0xAD; 0xBE; 0xEF]
          end
        form.file(
          "file" + i.string(),
          "f" + i.string() + ".bin",
          "application/octet-stream",
          data)
      end
      i = i + 1
    end

    let body_str = String.from_array(form.body())
    let ct = form.content_type()
    let boundary_prefix = "multipart/form-data; boundary="
    let boundary: String val = ct.substring(boundary_prefix.size().isize())

    // Count boundary occurrences
    let boundary_marker: String val = "--" + boundary
    var count: USize = 0
    var pos: ISize = 0
    while true do
      try
        let p = body_str.find(boundary_marker where offset = pos)?
        count = count + 1
        pos = p + boundary_marker.size().isize()
      else
        break
      end
    end
    // N parts means N opening boundaries + 1 closing boundary
    ph.assert_eq[USize](
      arg1 + 1,
      count,
      "should have N+1 boundary occurrences")

    // Verify each field name appears in the body
    i = 0
    while i < arg1 do
      if (i % 2) == 0 then
        ph.assert_true(
          body_str.contains("name=\"field" + i.string() + "\""),
          "field name should appear in body")
      else
        ph.assert_true(
          body_str.contains("name=\"file" + i.string() + "\""),
          "file name should appear in body")
        ph.assert_true(
          body_str.contains("filename=\"f" + i.string() + ".bin\""),
          "filename should appear in body")
      end
      i = i + 1
    end

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestMultipartBoundaryFormat is UnitTest
  """
  The boundary in content_type() starts with "----pony" and has total
  length 40 (8 prefix + 32 hex chars).
  """
  fun name(): String => "multipart/boundary_format"

  fun apply(h: TestHelper) =>
    let form = MultipartFormData
    let ct = form.content_type()
    let prefix = "multipart/form-data; boundary="
    let ct_prefix: String val = ct.substring(0, prefix.size().isize())
    h.assert_eq[String val](
      prefix,
      ct_prefix,
      "content_type should start with media type and boundary param")
    let boundary: String val = ct.substring(prefix.size().isize())
    let boundary_prefix: String val = boundary.substring(0, 8)
    h.assert_eq[String val](
      "----pony",
      boundary_prefix,
      "boundary should start with ----pony")
    h.assert_eq[USize](
      40,
      boundary.size(),
      "boundary should be 8 prefix + 32 hex = 40 chars")
    // Verify all hex chars after prefix
    let hex_part: String val = boundary.substring(8)
    for c in hex_part.values() do
      h.assert_true(
        ((c >= '0') and (c <= '9')) or ((c >= 'a') and (c <= 'f')),
        "boundary suffix should be lowercase hex")
    end

class \nodoc\ iso _TestMultipartContentTypeBoundaryConsistency is UnitTest
  """
  The boundary string in content_type() matches what appears in body().
  """
  fun name(): String => "multipart/content_type_boundary_consistency"

  fun apply(h: TestHelper) =>
    let form = MultipartFormData
    form.field("test", "value")

    let ct = form.content_type()
    let boundary_prefix = "multipart/form-data; boundary="
    let boundary: String val = ct.substring(boundary_prefix.size().isize())

    let body_str = String.from_array(form.body())
    let opening: String val = "--" + boundary + "\r\n"
    let closing: String val = "--" + boundary + "--\r\n"
    h.assert_true(
      body_str.contains(opening),
      "body should contain opening boundary from content_type")
    h.assert_true(
      body_str.contains(closing),
      "body should contain closing boundary from content_type")

class \nodoc\ iso _TestMultipartTextField is UnitTest
  """Single text field has correct Content-Disposition and body content."""
  fun name(): String => "multipart/text_field"

  fun apply(h: TestHelper) =>
    let form = MultipartFormData
    form.field("greeting", "hello world")
    let body_str = String.from_array(form.body())

    h.assert_true(
      body_str.contains("Content-Disposition: form-data; name=\"greeting\""),
      "should have Content-Disposition with field name")
    // Text fields should NOT have a Content-Type header
    h.assert_false(
      body_str.contains("Content-Type:"),
      "text field should not have Content-Type header")
    h.assert_true(
      body_str.contains("hello world"),
      "body should contain field value")

class \nodoc\ iso _TestMultipartFilePart is UnitTest
  """
  Single file has correct Content-Disposition with filename, Content-Type,
  and binary data preserved.
  """
  fun name(): String => "multipart/file_part"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = recover val [as U8: 0x89; 0x50; 0x4E; 0x47] end
    let form = MultipartFormData
    form.file("avatar", "photo.png", "image/png", data)
    let body = form.body()
    let body_str = String.from_array(body)

    h.assert_true(
      body_str.contains(
        "Content-Disposition: form-data;" +
        " name=\"avatar\"; filename=\"photo.png\""),
      "should have Content-Disposition with name and filename")
    h.assert_true(
      body_str.contains("Content-Type: image/png"),
      "should have Content-Type header")

    // Verify binary data is preserved byte-for-byte by searching for the
    // 4-byte sequence in the raw body bytes
    var found = false
    var i: USize = 0
    while i <= (body.size() - 4) do
      try
        if (body(i)? == 0x89) and (body(i + 1)? == 0x50) and
          (body(i + 2)? == 0x4E) and (body(i + 3)? == 0x47)
        then
          found = true
          break
        end
      end
      i = i + 1
    end
    h.assert_true(found, "binary file data should be preserved byte-for-byte")

class \nodoc\ iso _TestMultipartMixed is UnitTest
  """Multiple fields and files in order with correct boundary delimiters."""
  fun name(): String => "multipart/mixed"

  fun apply(h: TestHelper) =>
    let file_data: Array[U8] val = recover val [as U8: 1; 2; 3] end
    let form = MultipartFormData
      .> field("name", "alice")
      .> file("doc", "readme.txt", "text/plain", file_data)
      .> field("action", "upload")
    let body_str = String.from_array(form.body())

    // All three parts should be present
    h.assert_true(
      body_str.contains("name=\"name\""),
      "first field should be present")
    h.assert_true(
      body_str.contains("name=\"doc\""),
      "file should be present")
    h.assert_true(
      body_str.contains("name=\"action\""),
      "second field should be present")

    // Parts should appear in order
    try
      let name_pos = body_str.find("name=\"name\"")?
      let doc_pos = body_str.find("name=\"doc\"")?
      let action_pos = body_str.find("name=\"action\"")?
      h.assert_true(name_pos < doc_pos, "name should come before doc")
      h.assert_true(doc_pos < action_pos, "doc should come before action")
    else
      h.fail("all parts should be found")
    end

    // Closing boundary
    let ct = form.content_type()
    let boundary: String val =
      ct.substring(
        "multipart/form-data; boundary=".size().isize())
    let closing: String val = "--" + boundary + "--"
    h.assert_true(
      body_str.contains(closing),
      "should end with closing boundary")

class \nodoc\ iso _TestMultipartEmpty is UnitTest
  """Empty form produces just closing boundary."""
  fun name(): String => "multipart/empty"

  fun apply(h: TestHelper) =>
    let form = MultipartFormData
    let body_str = String.from_array(form.body())
    let ct = form.content_type()
    let boundary: String val =
      ct.substring(
        "multipart/form-data; boundary=".size().isize())

    let expected: String val = "--" + boundary + "--\r\n"
    h.assert_eq[String val](expected, body_str)

class \nodoc\ iso _TestMultipartBuilderIntegration is UnitTest
  """multipart_body() on request builder sets both body and Content-Type."""
  fun name(): String => "multipart/builder_integration"

  fun apply(h: TestHelper) =>
    let form = MultipartFormData
    form.field("key", "value")
    let expected_ct = form.content_type()
    let expected_body = form.body()

    let req = Request.post("/upload")
      .multipart_body(form)
      .build()

    // Content-Type header should match
    h.assert_eq[String val](
      expected_ct,
      match req.headers.get("Content-Type")
      | let v: String val => v
      else ""
      end)

    // Body should match
    match req.body
    | let b: Array[U8] val =>
      h.assert_eq[USize](expected_body.size(), b.size())
    else
      h.fail("body should be set")
    end

class \nodoc\ iso _TestMultipartNonAsciiFilename is UnitTest
  """Raw UTF-8 filename is preserved in Content-Disposition."""
  fun name(): String => "multipart/non_ascii_filename"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = recover val [as U8: 0xFF] end
    let form = MultipartFormData
    form.file("doc", "\xC3\xA9l\xC3\xA8ve.pdf", "application/pdf", data)
    let body_str = String.from_array(form.body())
    h.assert_true(
      body_str.contains("filename=\"\xC3\xA9l\xC3\xA8ve.pdf\""),
      "UTF-8 filename should be preserved as-is")

// ---------------------------------------------------------------------------
// Escaped quoting tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _PropertyMultipartEscapedNamesWellFormed
  is Property1[String val]
  """
  For any generated string used as a field name, the serialized body's
  Content-Disposition quoted-strings are well-formed: each opening `"`
  after `name=` is terminated by a closing `"` with only escaped quotes
  inside.
  """
  fun name(): String => "multipart/escaped_names_well_formed"

  fun gen(): Generator[String val] =>
    // Biased byte generator: extra copies of " and \ mixed with normal chars
    let byte_gen =
      Generators.one_of[U8](
        [ '"'; '"'; '"'
          '\\'; '\\'; '\\'
          'a'; 'b'; 'c'; 'd'; '.'; ' '; '=' ])
    Generators.byte_string(byte_gen, 1, 20)

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let form = MultipartFormData
    form.field(arg1, "val")
    let data: Array[U8] val = recover val [as U8: 0x00] end
    form.file(arg1, arg1, "application/octet-stream", data)

    let body_str = String.from_array(form.body())
    // Find each name=" and filename=" occurrence and verify well-formed
    // quoted-string
    let markers: Array[String val] val = ["name=\""; "filename=\""]
    for marker in markers.values() do
      var search_from: ISize = 0
      while true do
        let pos =
          try
            body_str.find(marker where offset = search_from)?
          else
            break
          end
        // Walk from after the opening " to find the closing "
        let start = pos + marker.size().isize()
        var i = start.usize()
        var escaped = false
        var found_close = false
        while i < body_str.size() do
          try
            let byte = body_str(i)?
            if escaped then
              escaped = false
            elseif byte == '\\' then
              escaped = true
            elseif byte == '"' then
              found_close = true
              break
            end
          end
          i = i + 1
        end
        ph.assert_true(
          found_close,
          "quoted-string after " + marker + " should have closing quote")
        ph.assert_false(
          escaped,
          "quoted-string should not end in a dangling backslash")
        search_from = i.isize() + 1
      end
    end

class \nodoc\ iso _TestMultipartFieldNameWithQuote is UnitTest
  """Field name containing `"` is escaped as `\"`."""
  fun name(): String => "multipart/field_name_with_quote"

  fun apply(h: TestHelper) =>
    let form = MultipartFormData
    form.field("field\"name", "value")
    let body_str = String.from_array(form.body())
    h.assert_true(
      body_str.contains("name=\"field\\\"name\""),
      "quote in field name should be escaped")

class \nodoc\ iso _TestMultipartFilenameWithQuote is UnitTest
  """Filename containing `"` is escaped as `\"`."""
  fun name(): String => "multipart/filename_with_quote"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = recover val [as U8: 0xFF] end
    let form = MultipartFormData
    form.file("doc", "file\"name.txt", "text/plain", data)
    let body_str = String.from_array(form.body())
    h.assert_true(
      body_str.contains("filename=\"file\\\"name.txt\""),
      "quote in filename should be escaped")

class \nodoc\ iso _TestMultipartFieldNameWithBackslash is UnitTest
  """Field name containing `\` is escaped as `\\`."""
  fun name(): String => "multipart/field_name_with_backslash"

  fun apply(h: TestHelper) =>
    let form = MultipartFormData
    form.field("field\\name", "value")
    let body_str = String.from_array(form.body())
    h.assert_true(
      body_str.contains("name=\"field\\\\name\""),
      "backslash in field name should be escaped")

class \nodoc\ iso _TestMultipartFilenameWithBothSpecials is UnitTest
  """Filename with both `"` and `\` has both escaped."""
  fun name(): String => "multipart/filename_with_both_specials"

  fun apply(h: TestHelper) =>
    let data: Array[U8] val = recover val [as U8: 0xFF] end
    let form = MultipartFormData
    form.file("doc", "a\\b\"c.txt", "text/plain", data)
    let body_str = String.from_array(form.body())
    h.assert_true(
      body_str.contains("filename=\"a\\\\b\\\"c.txt\""),
      "both backslash and quote in filename should be escaped")
