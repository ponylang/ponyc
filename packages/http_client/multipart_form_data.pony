use "format"
use "random"
use "time"

class ref MultipartFormData
  """
  Build a `multipart/form-data` body for file uploads and mixed form
  submissions (RFC 7578).

  Use `field()` for plain text values and `file()` for file attachments.
  After adding all parts, pass the builder to `multipart_body()` on the
  request builder, which sets both the body and `Content-Type` header.

  For simple key-value form data without files, use `FormEncoder` with
  `form_body()` instead — it produces the more compact
  `application/x-www-form-urlencoded` format.

  ```pony
  let form = MultipartFormData
    .> field("username", "alice")
    .> file("avatar", "photo.jpg", "image/jpeg", image_data)
  let req = Request.post("/upload")
    .multipart_body(form)
    .build()
  ```
  """
  let _boundary: String
  embed _parts: Array[_MultipartPart]

  new ref create() =>
    """
    Create a new builder with a randomly generated boundary string.
    """
    _parts = Array[_MultipartPart]
    (let secs, let nanos) = Time.now()
    let rand = Rand(secs.u64(), nanos.u64())
    let a = rand.next()
    let b = rand.next()
    _boundary = "----pony" +
      Format.int[U64](a, FormatHexSmallBare where width = 16, fill = '0') +
      Format.int[U64](b, FormatHexSmallBare where width = 16, fill = '0')

  fun ref field(name: String, value: String): MultipartFormData ref =>
    """
    Add a text field to the form.

    `"` and `\` in the name are automatically backslash-escaped in the
    serialized `Content-Disposition` quoted-string.
    """
    _parts.push(_MultipartField(name, value))
    this

  fun ref file(
    name: String,
    filename: String,
    file_content_type: String,
    data: Array[U8] val)
    : MultipartFormData ref
  =>
    """
    Add a file attachment to the form.

    `"` and `\` in the name and filename are automatically backslash-escaped
    in the serialized `Content-Disposition` quoted-string.
    """
    _parts.push(_MultipartFile(name, filename, file_content_type, data))
    this

  fun content_type(): String =>
    """
    Return the `Content-Type` header value including the boundary parameter.

    Pass this to the request's `Content-Type` header so the server knows how
    to parse the body. The `multipart_body()` method on the request builder
    does this automatically.
    """
    "multipart/form-data; boundary=" + _boundary

  fun body(): Array[U8] val =>
    """
    Serialize all parts into the `multipart/form-data` wire format.

    Each part is delimited by the boundary string. Field parts include a
    `Content-Disposition` header; file parts additionally include a `filename`
    parameter and a `Content-Type` header. The body ends with a closing
    boundary.

    Field names and filenames are backslash-escaped (`"` becomes `\"`, `\`
    becomes `\\`) within `Content-Disposition` quoted-strings.
    """
    let buf = recover iso Array[U8] end
    for part in _parts.values() do
      buf.append("--")
      buf.append(_boundary)
      buf.append("\r\n")
      match \exhaustive\ part
      | let f: _MultipartField val =>
        buf.append("Content-Disposition: form-data; name=\"")
        buf.append(_escape_quoted(f.name))
        buf.append("\"\r\n\r\n")
        buf.append(f.value)
      | let f: _MultipartFile val =>
        buf.append("Content-Disposition: form-data; name=\"")
        buf.append(_escape_quoted(f.name))
        buf.append("\"; filename=\"")
        buf.append(_escape_quoted(f.filename))
        buf.append("\"\r\nContent-Type: ")
        buf.append(f.content_type)
        buf.append("\r\n\r\n")
        buf.append(f.data)
      end
      buf.append("\r\n")
    end
    buf.append("--")
    buf.append(_boundary)
    buf.append("--\r\n")
    consume buf

  fun _escape_quoted(input: String): String iso^ =>
    let buf = recover iso String(input.size()) end
    for byte in input.values() do
      if (byte == '"') or (byte == '\\') then
        buf.push('\\')
      end
      buf.push(byte)
    end
    consume buf
