primitive \nodoc\ _ClientSpelling
  """
  Spells a path the way an LSP client does, without going through `Uris`.

  A test that built its URIs with `Uris.from_path` would still pass if
  encoding and decoding were broken in matching ways, because the two would
  cancel out. Spelling the URI here keeps the client's side independent of the
  code under test.

  The Windows spelling is VS Code's: forward slashes, and an escaped colon
  after the drive letter.
  """
  fun uri(path: String): String =>
    let s = path.clone()
    ifdef windows then
      s.replace("\\", "/")
      s.replace(":", "%3A")
    end
    s.replace(" ", "%20")
    let prefix = ifdef windows then "file:///" else "file://" end
    prefix + consume s
