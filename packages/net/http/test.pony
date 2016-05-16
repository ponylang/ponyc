use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_Encode)
    test(_EncodeBad)
    test(_EncodeIPv6)
    test(_EncodeClean)

    test(_Check)
    test(_CheckBad)
    test(_CheckScheme)
    test(_CheckIPv6)

    test(_Decode)
    test(_DecodeBad)

    test(_BuildBasic)
    test(_BuildMissingParts)
    test(_BuildBad)
    test(_BuildNoEncoding)
    test(_Valid)
    test(_ToStringFun)

    test(_Client)


class iso _Encode is UnitTest
  fun name(): String => "net/http/URLEncode.encode"

  fun apply(h: TestHelper) ? =>
    // Unreserved chars, decoded.
    h.assert_eq[String]("Aa4-._~Aa4-._~",
      URLEncode.encode("Aa4-._~%41%61%34%2D%2E%5F%7E", URLPartUser))

    h.assert_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartPassword))
    h.assert_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartHost))
    h.assert_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartPath))
    h.assert_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartQuery))
    h.assert_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartFragment))

    // Sub-delimiters, left encoded or not as original.
    h.assert_eq[String]("!$&'()*+,;=%21%24%26%27%28%29%2A%2B%2C%3B%3D",
      URLEncode.encode("!$&'()*+,;=%21%24%26%27%28%29%2A%2B%2C%3B%3D",
        URLPartUser))

    h.assert_eq[String](",%2C", URLEncode.encode(",%2C", URLPartPassword))
    h.assert_eq[String](",%2C", URLEncode.encode(",%2C", URLPartHost))
    h.assert_eq[String](",%2C", URLEncode.encode(",%2C", URLPartPath))
    h.assert_eq[String](",%2C", URLEncode.encode(",%2C", URLPartQuery))
    h.assert_eq[String](",%2C", URLEncode.encode(",%2C", URLPartFragment))

    // Misc characters, encoded.
    h.assert_eq[String]("%23%3C%3E%5B%5D%7B%7D%7C%5E%20" +
      "%23%3C%3E%5B%5D%7B%7D%7C%5E%25",
      URLEncode.encode("#<>[]{}|^ %23%3C%3E%5B%5D%7B%7D%7C%5E%25",
        URLPartUser))

    h.assert_eq[String]("%23%23", URLEncode.encode("#%23", URLPartPassword))
    h.assert_eq[String]("%23%23", URLEncode.encode("#%23", URLPartHost))
    h.assert_eq[String]("%23%23", URLEncode.encode("#%23", URLPartPath))
    h.assert_eq[String]("%23%23", URLEncode.encode("#%23", URLPartQuery))
    h.assert_eq[String]("%23%23", URLEncode.encode("#%23", URLPartFragment))

    // Delimiters, whether encoded depends on URL part.
    h.assert_eq[String]("%3A%40%2F%3F", URLEncode.encode(":@/?", URLPartUser))
    h.assert_eq[String](":%40%2F%3F",
      URLEncode.encode(":@/?", URLPartPassword))
    h.assert_eq[String]("%3A%40%2F%3F", URLEncode.encode(":@/?", URLPartHost))
    h.assert_eq[String](":@/%3F", URLEncode.encode(":@/?", URLPartPath))
    h.assert_eq[String](":@/?", URLEncode.encode(":@/?", URLPartQuery))
    h.assert_eq[String](":@/?", URLEncode.encode(":@/?", URLPartFragment))

class iso _EncodeBad is UnitTest
  fun name(): String => "net/http/URLEncode.encode_bad"

  fun apply(h: TestHelper) =>
    h.assert_error(lambda()? => URLEncode.encode("%2G", URLPartUser) end)
    h.assert_error(lambda()? => URLEncode.encode("%xx", URLPartUser) end)
    h.assert_error(lambda()? => URLEncode.encode("%2", URLPartUser) end)

class iso _EncodeIPv6 is UnitTest
  fun name(): String => "net/http/URLEncode.encode_ipv6"

  fun apply(h: TestHelper) ? =>
    // Allowed hex digits, '.' and ':' only, between '[' and ']'.
    h.assert_eq[String]("[1::A.B]", URLEncode.encode("[1::A.B]", URLPartHost))
    h.assert_error(lambda()? => URLEncode.encode("[G]", URLPartHost) end)
    h.assert_error(lambda()? => URLEncode.encode("[/]", URLPartHost) end)
    h.assert_error(lambda()? => URLEncode.encode("[%32]", URLPartHost) end)
    h.assert_error(lambda()? => URLEncode.encode("[1]2", URLPartHost) end)
    h.assert_error(lambda()? => URLEncode.encode("[1", URLPartHost) end)
    h.assert_eq[String]("1%5D", URLEncode.encode("1]", URLPartHost))

class iso _EncodeClean is UnitTest
  fun name(): String => "net/http/URLEncode.encode_clean"

  fun apply(h: TestHelper) ? =>
    // No percent encoding in source string.
    h.assert_eq[String]("F_1x", URLEncode.encode("F_1x", URLPartQuery, false))
    h.assert_eq[String]("%2541", URLEncode.encode("%41", URLPartQuery, false))
    h.assert_eq[String]("%25", URLEncode.encode("%", URLPartQuery, false))

class iso _Check is UnitTest
  fun name(): String => "net/http/URLEncode.check"

  fun apply(h: TestHelper) =>
    // Unreserved chars, legal encoded or not.
    h.assert_eq[Bool](true,
      URLEncode.check("Aa4-._~%41%61%34%2D%2E%5F%7E", URLPartUser))

    h.assert_eq[Bool](true, URLEncode.check("F_1%32x", URLPartPassword))
    h.assert_eq[Bool](true, URLEncode.check("F_1%32x", URLPartHost))
    h.assert_eq[Bool](true, URLEncode.check("F_1%32x", URLPartPath))
    h.assert_eq[Bool](true, URLEncode.check("F_1%32x", URLPartQuery))
    h.assert_eq[Bool](true, URLEncode.check("F_1%32x", URLPartFragment))

    // Sub-delimiters, legal encoded or not.
    h.assert_eq[Bool](true,
      URLEncode.check("!$&'()*+,;=%21%24%26%27%28%29%2A%2B%2C%3B%3D",
        URLPartUser))

    h.assert_eq[Bool](true, URLEncode.check(",%2C", URLPartPassword))
    h.assert_eq[Bool](true, URLEncode.check(",%2C", URLPartHost))
    h.assert_eq[Bool](true, URLEncode.check(",%2C", URLPartPath))
    h.assert_eq[Bool](true, URLEncode.check(",%2C", URLPartQuery))
    h.assert_eq[Bool](true, URLEncode.check(",%2C", URLPartFragment))

    // Misc characters, must be encoded.
    h.assert_eq[Bool](true,
      URLEncode.check("%23%3C%3E%5B%5D%7B%7D%7C%5E%25", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check("<", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check(">", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check("|", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check("^", URLPartUser))

    h.assert_eq[Bool](true, URLEncode.check("%23%3C", URLPartPassword))
    h.assert_eq[Bool](false, URLEncode.check("<", URLPartPassword))
    h.assert_eq[Bool](true, URLEncode.check("%23%3C", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("<", URLPartHost))
    h.assert_eq[Bool](true, URLEncode.check("%23%3C", URLPartPath))
    h.assert_eq[Bool](false, URLEncode.check("<", URLPartPath))
    h.assert_eq[Bool](true, URLEncode.check("%23%3C", URLPartQuery))
    h.assert_eq[Bool](false, URLEncode.check("<", URLPartQuery))
    h.assert_eq[Bool](true, URLEncode.check("%23%3C", URLPartFragment))
    h.assert_eq[Bool](false, URLEncode.check("<", URLPartFragment))

    // Delimiters, whether need to be encoded depends on URL part.
    h.assert_eq[Bool](true, URLEncode.check("%3A%40%2F%3F", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check(":", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check("@", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check("/", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check("?", URLPartUser))
    h.assert_eq[Bool](true, URLEncode.check(":%40%2F%3F", URLPartPassword))
    h.assert_eq[Bool](false, URLEncode.check("@", URLPartPassword))
    h.assert_eq[Bool](false, URLEncode.check("/", URLPartPassword))
    h.assert_eq[Bool](false, URLEncode.check("?", URLPartPassword))
    h.assert_eq[Bool](true, URLEncode.check("%3A%40%2F%3F", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check(":", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("@", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("/", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("?", URLPartHost))
    h.assert_eq[Bool](true, URLEncode.check(":@/%3F", URLPartPath))
    h.assert_eq[Bool](false, URLEncode.check("?", URLPartPath))
    h.assert_eq[Bool](true, URLEncode.check(":@/?", URLPartQuery))
    h.assert_eq[Bool](true, URLEncode.check(":@/?", URLPartFragment))

class iso _CheckBad is UnitTest
  fun name(): String => "net/http/URLEncode.check_bad"

  fun apply(h: TestHelper) =>
    h.assert_eq[Bool](false, URLEncode.check("%2G", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check("%xx", URLPartUser))
    h.assert_eq[Bool](false, URLEncode.check("%2", URLPartUser))

class iso _CheckScheme is UnitTest
  fun name(): String => "net/http/URLEncode.check_scheme"

  fun apply(h: TestHelper) =>
    h.assert_eq[Bool](true, URLEncode.check_scheme("Aa4-+."))
    h.assert_eq[Bool](false, URLEncode.check_scheme("_"))
    h.assert_eq[Bool](false, URLEncode.check_scheme(":"))
    h.assert_eq[Bool](false, URLEncode.check_scheme("%41"))

class iso _CheckIPv6 is UnitTest
  fun name(): String => "net/http/URLEncode.check_ipv6"

  fun apply(h: TestHelper) =>
    // Allowed hex digits, '.' and ':' only, between '[' and ']'.
    h.assert_eq[Bool](true, URLEncode.check("[1::A.B]", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("[G]", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("[/]", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("[%32]", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("[1]2", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("[1", URLPartHost))
    h.assert_eq[Bool](false, URLEncode.check("1]", URLPartHost))

class iso _Decode is UnitTest
  fun name(): String => "net/http/URLEncode.decode"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[String]("Aa4-._~Aa4-._~",
      URLEncode.decode("Aa4-._~%41%61%34%2D%2E%5F%7E"))

    h.assert_eq[String]("F_12x", URLEncode.decode("F_1%32x"))

    h.assert_eq[String]("!$&'()*+,;=!$&'()*+,;=",
      URLEncode.decode("!$&'()*+,;=%21%24%26%27%28%29%2A%2B%2C%3B%3D"))

    h.assert_eq[String]("#<>[]{}|^ #<>[]{}|^ %",
      URLEncode.decode("#<>[]{}|^ %23%3C%3E%5B%5D%7B%7D%7C%5E%20%25"))

class iso _DecodeBad is UnitTest
  fun name(): String => "net/http/URLEncode.decode_bad"

  fun apply(h: TestHelper) =>
    h.assert_error(lambda()? => URLEncode.decode("%2G") end)
    h.assert_error(lambda()? => URLEncode.decode("%xx") end)
    h.assert_error(lambda()? => URLEncode.decode("%2") end)

class iso _BuildBasic is UnitTest
  fun name(): String => "net/http/URL.build_basic"

  fun apply(h: TestHelper) ? =>
    _Test(h,
      URL.build("https://user:password@host.name:12345/path?query#fragment"),
      "https", "user", "password", "host.name", 12345, "/path", "query",
      "fragment")

    _Test(h,
      URL.build("http://rosettacode.org/wiki/Category]Pony"),
      "http", "", "", "rosettacode.org", 80, "/wiki/Category%5DPony", "", "")

    _Test(h,
      URL.build("https://en.wikipedia.org/wiki/Polymorphism_" +
        "(computer_science)#Parametric_polymorphism"),
      "https", "", "", "en.wikipedia.org", 443,
      "/wiki/Polymorphism_(computer_science)", "",
      "Parametric_polymorphism")

    _Test(h, URL.build("http://user@host"),
      "http", "user", "", "host", 80, "/", "", "")

class iso _BuildMissingParts is UnitTest
  fun name(): String => "net/http/URL.build_missing_parts"

  fun apply(h: TestHelper) ? =>
    _Test(h, URL.build("https://user@host.name/path#fragment"),
      "https", "user", "", "host.name", 443, "/path", "", "fragment")

    _Test(h, URL.build("https://user@host.name#fragment"),
      "https", "user", "", "host.name", 443, "/", "", "fragment")

    _Test(h, URL.build("//host.name/path"),
      "", "", "", "host.name", 0, "/path", "", "")

    _Test(h, URL.build("/path"),
      "", "", "", "", 0, "/path", "", "")

    _Test(h, URL.build("?query"),
      "", "", "", "", 0, "/", "query", "")

    _Test(h, URL.build("#fragment"),
      "", "", "", "", 0, "/", "", "fragment")

    _Test(h, URL.build("https://host.name/path#frag?ment"),
      "https", "", "", "host.name", 443, "/path", "", "frag?ment")

    _Test(h, URL.build("https://user@host.name?quer/y#fragment"),
      "https", "user", "", "host.name", 443, "/", "quer/y", "fragment")

class iso _BuildBad is UnitTest
  fun name(): String => "net/http/URL.build_bad"

  fun apply(h: TestHelper) =>
    h.assert_error(lambda()? =>
      URL.build("htt_ps://user@host.name/path#fragment") end)

    h.assert_error(lambda()? =>
      URL.build("https://[11::24_]/path") end)

    h.assert_error(lambda()? =>
      URL.build("https://[11::24/path") end)

    h.assert_error(lambda()? =>
      URL.build("https://host%2Gname/path") end)

    h.assert_error(lambda()? =>
      URL.build("https://hostname/path%") end)

class iso _BuildNoEncoding is UnitTest
  fun name(): String => "net/http/URL.build_no_encoding"

  fun apply(h: TestHelper) ? =>
    _Test(h, URL.build("https://host.name/path%32path", false),
      "https", "", "", "host.name", 443, "/path%2532path", "", "")

class iso _Valid is UnitTest
  fun name(): String => "net/http/URL.valid"

  fun apply(h: TestHelper) ? =>
    _Test(h,
      URL.valid("https://user:password@host.name:12345/path?query#fragment"),
      "https", "user", "password", "host.name", 12345, "/path", "query",
      "fragment")

    h.assert_error(lambda()? =>
      URL.valid("http://rosettacode.org/wiki/Category[Pony]") end)

    h.assert_error(lambda()? =>
      URL.valid("https://en.wikipedia|org/wiki/Polymorphism_" +
        "(computer_science)#Parametric_polymorphism") end)

    _Test(h, URL.valid("http://user@host"),
      "http", "user", "", "host", 80, "/", "", "")

class iso _ToStringFun is UnitTest
  fun name(): String => "net/http/URL.to_string"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[String](
      "https://user:password@host.name:12345/path?query#fragment",
      URL.build("https://user:password@host.name:12345/path?query#fragment")
        .string())

    h.assert_eq[String]("http://rosettacode.org/wiki/Category%5DPony",
      URL.build("http://rosettacode.org/wiki/Category]Pony").string())

    h.assert_eq[String]("http://user@host/",
      URL.build("http://user@host").string())

    // Default ports should be omitted.
    h.assert_eq[String]("http://host.name/path",
      URL.build("http://host.name:80/path").string())

class iso _Client is UnitTest
  fun name(): String => "net/http/Client"

  fun apply(h: TestHelper) ? =>
    h.long_test(10_000_000_000)
    let client = Client(h.env.root as AmbientAuth)
    let url = URL.build("https://httpbin.org/get")
    let handler = object val
      let h: TestHelper = h
      fun apply(req: Payload val, res: Payload val) =>
        if res.status == 0 then h.fail() else None end
        try
          h.log(res.string())
          h.assert_eq[String]("things", req("stuff"))
          h.assert_eq[String]("text/html", req("Content-Type"))
          h.assert_eq[String]("Basic YXdlc29tZV91c2VyOjEyMzQ1Ng==\r\n",
            req("Authentication"))
        else
          h.fail()
        end
        h.complete(true)
    end
    client.get(url)
      .set("stuff", "things")
      .content_type(Content.html())
      .basic_auth("awesome_user", "123456")
      .send(handler)

primitive _Test
  fun apply(h: TestHelper, url: URL, scheme: String, user: String,
    password: String, host: String, port: U16, path: String,
    query: String, fragment: String)
  =>
    h.assert_eq[String](scheme, url.scheme)
    h.assert_eq[String](user, url.user)
    h.assert_eq[String](password, url.password)
    h.assert_eq[String](host, url.host)
    h.assert_eq[U16](port, url.port)
    h.assert_eq[String](path, url.path)
    h.assert_eq[String](query, url.query)
    h.assert_eq[String](fragment, url.fragment)
