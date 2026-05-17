use "pony_test"
use ".."

primitive _URITests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_URIToPathTest)
    test(_URIToPathNoSchemeTest)
    test(_URIFromPathTest)
    test(_URIFromPathAlreadyURITest)

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
  When no "://" separator is present, split_by returns the whole string
  as a single element and to_path returns it unchanged.
  """
  fun name(): String => "uris/to_path/no_scheme"

  fun apply(h: TestHelper) =>
    ifdef windows then
      h.assert_eq[String](
        "C:\\foo\\bar.pony",
        Uris.to_path("C:\\foo\\bar.pony"))
    else
      h.assert_eq[String](
        "/foo/bar.pony",
        Uris.to_path("/foo/bar.pony"))
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

class \nodoc\ iso _URIFromPathAlreadyURITest is UnitTest
  fun name(): String => "uris/from_path/already_uri"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "file:///foo/bar.pony",
      Uris.from_path("file:///foo/bar.pony"))
