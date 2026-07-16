use "pony_check"
use "pony_test"
use ".."

primitive _URITests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_URIToPathTest)
    test(_URIToPathNoSchemeTest)
    test(_URIToPathDecodeTest)
    test(_URIFromPathTest)
    test(_URIFromPathEncodeTest)
    test(_URIFromPathAlreadyURITest)
    test(_URIRoundTripTest)
    test(_URIRoundTripPropertyTest)

class \nodoc\ iso _URIToPathTest is UnitTest
  fun name(): String => "uris/to_path"

  fun apply(h: TestHelper) =>
    ifdef windows then
      h.assert_eq[String](
        "D:\\foo\\bar.pony",
        Uris.to_path("file:///D:/foo/bar.pony"))
      h.assert_eq[String](
        "\\foo\\bar.pony",
        Uris.to_path("file:///foo/bar.pony"))
      h.assert_eq[String](
        "\\",
        Uris.to_path("file:///"))
    else
      h.assert_eq[String]("/foo/bar.pony", Uris.to_path("file:///foo/bar.pony"))
    end

class \nodoc\ iso _URIToPathNoSchemeTest is UnitTest
  """
  A string with no "://" is returned unchanged. Unchanged means undecoded: a
  path is bytes, so a '%' in one is part of a filename.
  """
  fun name(): String => "uris/to_path/no_scheme"

  fun apply(h: TestHelper) =>
    ifdef windows then
      h.assert_eq[String](
        "C:\\foo\\bar.pony",
        Uris.to_path("C:\\foo\\bar.pony"))
      h.assert_eq[String](
        "C:\\foo\\50%20off.pony",
        Uris.to_path("C:\\foo\\50%20off.pony"))
    else
      h.assert_eq[String](
        "/foo/bar.pony",
        Uris.to_path("/foo/bar.pony"))
      h.assert_eq[String](
        "/foo/50%20off.pony",
        Uris.to_path("/foo/50%20off.pony"))
    end

class \nodoc\ iso _URIToPathDecodeTest is UnitTest
  """
  Clients percent-encode the URIs they send. VS Code and emacs lsp-mode also
  encode the drive colon, so decoding has to run before the drive-letter check.
  """
  fun name(): String => "uris/to_path/decode"

  fun apply(h: TestHelper) =>
    ifdef windows then
      h.assert_eq[String](
        "c:\\Users\\sean\\x.pony",
        Uris.to_path("file:///c%3A/Users/sean/x.pony"))
      h.assert_eq[String](
        "C:\\project\\readme.md",
        Uris.to_path("file:///C%3A/project/readme.md"))
      // Neovim emits lowercase hex.
      h.assert_eq[String](
        "c:\\Users\\sean\\x.pony",
        Uris.to_path("file:///c%3a/Users/sean/x.pony"))
      h.assert_eq[String](
        "C:\\my project\\x.pony",
        Uris.to_path("file:///C:/my%20project/x.pony"))
    else
      h.assert_eq[String](
        "/home/sean/my project/x.pony",
        Uris.to_path("file:///home/sean/my%20project/x.pony"))
      h.assert_eq[String](
        "/home/sean/café/x.pony",
        Uris.to_path("file:///home/sean/caf%C3%A9/x.pony"))
      // Neovim emits lowercase hex.
      h.assert_eq[String](
        "/home/sean/café/x.pony",
        Uris.to_path("file:///home/sean/caf%c3%a9/x.pony"))
      h.assert_eq[String](
        "/a/100%.pony",
        Uris.to_path("file:///a/100%25.pony"))
      h.assert_eq[String](
        "/a/c#1.pony",
        Uris.to_path("file:///a/c%231.pony"))
      h.assert_eq[String](
        "/a/%ZZ.pony",
        Uris.to_path("file:///a/%ZZ.pony"))
    end

class \nodoc\ iso _URIFromPathTest is UnitTest
  fun name(): String => "uris/from_path"

  fun apply(h: TestHelper) =>
    ifdef windows then
      h.assert_eq[String](
        "file:///D:/foo/bar.pony",
        Uris.from_path("D:\\foo\\bar.pony"))
    else
      h.assert_eq[String](
        "file:///foo/bar.pony",
        Uris.from_path("/foo/bar.pony"))
    end

class \nodoc\ iso _URIFromPathEncodeTest is UnitTest
  """
  A raw space is not a legal URI at all, and a raw '#' would be read back as
  the start of a fragment, naming a different file.
  """
  fun name(): String => "uris/from_path/encode"

  fun apply(h: TestHelper) =>
    ifdef windows then
      h.assert_eq[String](
        "file:///C:/my%20project/x.pony",
        Uris.from_path("C:\\my project\\x.pony"))
      h.assert_eq[String](
        "file:///C:/a/100%25.pony",
        Uris.from_path("C:\\a\\100%.pony"))
    else
      h.assert_eq[String](
        "file:///home/sean/my%20project/x.pony",
        Uris.from_path("/home/sean/my project/x.pony"))
      h.assert_eq[String](
        "file:///home/sean/caf%C3%A9/x.pony",
        Uris.from_path("/home/sean/café/x.pony"))
      h.assert_eq[String](
        "file:///a/100%25.pony",
        Uris.from_path("/a/100%.pony"))
      h.assert_eq[String](
        "file:///a/c%231.pony",
        Uris.from_path("/a/c#1.pony"))
      // pchar stays literal, so these keep the spelling clients already send.
      h.assert_eq[String](
        "file:///a/b,c+d.pony",
        Uris.from_path("/a/b,c+d.pony"))
    end

class \nodoc\ iso _URIFromPathAlreadyURITest is UnitTest
  """
  A string that is already a URI passes through, escapes and all, rather than
  having its '%' encoded a second time.
  """
  fun name(): String => "uris/from_path/already_uri"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "file:///foo/bar.pony",
      Uris.from_path("file:///foo/bar.pony"))
    h.assert_eq[String](
      "file:///a%20b.pony",
      Uris.from_path("file:///a%20b.pony"))

class \nodoc\ iso _URIRoundTripTest is UnitTest
  """
  Diagnostic.from_error turns a compiler path into a URI with from_path and
  WorkspaceManager turns it back with to_path, so the pair has to be lossless.
  A file named "50%20off.pony" comes back as "50 off.pony" if the encode side
  does not escape '%'.
  """
  fun name(): String => "uris/round_trip"

  fun apply(h: TestHelper) =>
    let cases =
      ifdef windows then
        [ "C:\\a\\my project\\x.pony"; "C:\\a\\100%.pony"
          "C:\\a\\50%20off.pony"; "C:\\a\\café.pony"; "C:\\a\\c#1.pony" ]
      else
        [ "/a/my project/x.pony"; "/a/100%.pony"; "/a/50%20off.pony"
          "/a/café.pony"; "/a/c#1.pony"; "/a/b,c+d.pony" ]
      end
    for path in cases.values() do
      h.assert_eq[String](
        path,
        Uris.to_path(Uris.from_path(path)),
        "round trip of " + path)
    end

class \nodoc\ iso _URIRoundTripPropertyTest is UnitTest
  """
  to_path undoes from_path for any single-segment absolute path.

  '/' is removed from the generated segment on both platforms: on Windows it
  comes back as a separator, and on either platform a segment holding "://"
  makes from_path treat the path as a URI already. '\' is removed only on
  Windows, where it is a separator; on Linux it is an ordinary filename byte
  and encodes to %5C. Windows needs the drive letter, since from_path only
  produces a round-trippable URI for a drive-letter path.
  """
  fun name(): String => "uris/round_trip/property"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[String](
      recover val
        Generators.ascii(where min = 0, max = 30, range = ASCIIPrintable)
      end,
      h)(
      {(segment: String, ph: PropertyHelper) =>
        let cleaned = segment.clone()
        cleaned.remove("/")
        ifdef windows then
          cleaned.remove("\\")
        end
        let safe: String val = consume cleaned
        let path: String val =
          ifdef windows then
            "C:\\tmp\\" + safe
          else
            "/tmp/" + safe
          end
        ph.assert_eq[String](path, Uris.to_path(Uris.from_path(path)))
      })?
