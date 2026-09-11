class val _MultipartField
  """
  A text field in a multipart form.
  """
  let name: String
  let value: String

  new val create(name': String, value': String) =>
    name = name'
    value = value'

class val _MultipartFile
  """
  A file attachment in a multipart form.
  """
  let name: String
  let filename: String
  let content_type: String
  let data: Array[U8] val

  new val create(
    name': String,
    filename': String,
    content_type': String,
    data': Array[U8] val)
  =>
    name = name'
    filename = filename'
    content_type = content_type'
    data = data'

type _MultipartPart is (_MultipartField | _MultipartFile)
  """A single part in a multipart form: either a text field or a file."""
