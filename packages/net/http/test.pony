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


class iso _Encode is UnitTest
  fun name(): String => "net/http/URLEncode.encode"

  fun apply(h: TestHelper): TestResult ? =>
    // Unreserved chars, decoded.
    h.expect_eq[String]("Aa4-._~Aa4-._~",
      URLEncode.encode("Aa4-._~%41%61%34%2D%2E%5F%7E", URLPartUser))

    h.expect_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartPassword))
    h.expect_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartHost))
    h.expect_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartPath))
    h.expect_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartQuery))
    h.expect_eq[String]("F_12x", URLEncode.encode("F_1%32x", URLPartFragment))

    // Sub-delimiters, left encoded or not as original.
    h.expect_eq[String]("!$&'()*+,;=%21%24%26%27%28%29%2A%2B%2C%3B%3D",
      URLEncode.encode("!$&'()*+,;=%21%24%26%27%28%29%2A%2B%2C%3B%3D",
        URLPartUser))

    h.expect_eq[String](",%2C", URLEncode.encode(",%2C", URLPartPassword))
    h.expect_eq[String](",%2C", URLEncode.encode(",%2C", URLPartHost))
    h.expect_eq[String](",%2C", URLEncode.encode(",%2C", URLPartPath))
    h.expect_eq[String](",%2C", URLEncode.encode(",%2C", URLPartQuery))
    h.expect_eq[String](",%2C", URLEncode.encode(",%2C", URLPartFragment))

    // Misc characters, encoded.
    h.expect_eq[String]("%23%3C%3E%5B%5D%7B%7D%7C%5E%20" +
      "%23%3C%3E%5B%5D%7B%7D%7C%5E%25",
      URLEncode.encode("#<>[]{}|^ %23%3C%3E%5B%5D%7B%7D%7C%5E%25",
        URLPartUser))
        
    h.expect_eq[String]("%23%23", URLEncode.encode("#%23", URLPartPassword))
    h.expect_eq[String]("%23%23", URLEncode.encode("#%23", URLPartHost))
    h.expect_eq[String]("%23%23", URLEncode.encode("#%23", URLPartPath))
    h.expect_eq[String]("%23%23", URLEncode.encode("#%23", URLPartQuery))
    h.expect_eq[String]("%23%23", URLEncode.encode("#%23", URLPartFragment))

    // Delimiters, whether encoded depends on URL part.
    h.expect_eq[String]("%3A%40%2F%3F", URLEncode.encode(":@/?", URLPartUser))
    h.expect_eq[String](":%40%2F%3F",
      URLEncode.encode(":@/?", URLPartPassword))
    h.expect_eq[String]("%3A%40%2F%3F", URLEncode.encode(":@/?", URLPartHost))
    h.expect_eq[String](":@/%3F", URLEncode.encode(":@/?", URLPartPath))
    h.expect_eq[String](":@/?", URLEncode.encode(":@/?", URLPartQuery))
    h.expect_eq[String](":@/?", URLEncode.encode(":@/?", URLPartFragment))

    true
    
class iso _EncodeBad is UnitTest
  fun name(): String => "net/http/URLEncode.encode_bad"

  fun apply(h: TestHelper): TestResult =>
    h.expect_error(lambda()? => URLEncode.encode("%2G", URLPartUser) end)
    h.expect_error(lambda()? => URLEncode.encode("%xx", URLPartUser) end)
    h.expect_error(lambda()? => URLEncode.encode("%2", URLPartUser) end)
    true
      
class iso _EncodeIPv6 is UnitTest
  fun name(): String => "net/http/URLEncode.encode_ipv6"

  fun apply(h: TestHelper): TestResult ? =>
    // Allowed hex digits, '.' and ':' only, between '[' and ']'.
    h.expect_eq[String]("[1::A.B]", URLEncode.encode("[1::A.B]", URLPartHost))
    h.expect_error(lambda()? => URLEncode.encode("[G]", URLPartHost) end)
    h.expect_error(lambda()? => URLEncode.encode("[/]", URLPartHost) end)
    h.expect_error(lambda()? => URLEncode.encode("[%32]", URLPartHost) end)
    h.expect_error(lambda()? => URLEncode.encode("[1]2", URLPartHost) end)
    h.expect_error(lambda()? => URLEncode.encode("[1", URLPartHost) end)
    h.expect_eq[String]("1%5D", URLEncode.encode("1]", URLPartHost))
    true

class iso _EncodeClean is UnitTest
  fun name(): String => "net/http/URLEncode.encode_clean"

  fun apply(h: TestHelper): TestResult ? =>
    // No percent encoding in source string.
    h.expect_eq[String]("F_1x", URLEncode.encode("F_1x", URLPartQuery, false))
    h.expect_eq[String]("%2541", URLEncode.encode("%41", URLPartQuery, false))
    h.expect_eq[String]("%25", URLEncode.encode("%", URLPartQuery, false))
    true

class iso _Check is UnitTest
  fun name(): String => "net/http/URLEncode.check"

  fun apply(h: TestHelper): TestResult =>
    // Unreserved chars, legal encoded or not.
    h.expect_eq[Bool](true,
      URLEncode.check("Aa4-._~%41%61%34%2D%2E%5F%7E", URLPartUser))

    h.expect_eq[Bool](true, URLEncode.check("F_1%32x", URLPartPassword))
    h.expect_eq[Bool](true, URLEncode.check("F_1%32x", URLPartHost))
    h.expect_eq[Bool](true, URLEncode.check("F_1%32x", URLPartPath))
    h.expect_eq[Bool](true, URLEncode.check("F_1%32x", URLPartQuery))
    h.expect_eq[Bool](true, URLEncode.check("F_1%32x", URLPartFragment))

    // Sub-delimiters, legal encoded or not.
    h.expect_eq[Bool](true,
      URLEncode.check("!$&'()*+,;=%21%24%26%27%28%29%2A%2B%2C%3B%3D",
        URLPartUser))

    h.expect_eq[Bool](true, URLEncode.check(",%2C", URLPartPassword))
    h.expect_eq[Bool](true, URLEncode.check(",%2C", URLPartHost))
    h.expect_eq[Bool](true, URLEncode.check(",%2C", URLPartPath))
    h.expect_eq[Bool](true, URLEncode.check(",%2C", URLPartQuery))
    h.expect_eq[Bool](true, URLEncode.check(",%2C", URLPartFragment))

    // Misc characters, must be encoded.
    h.expect_eq[Bool](true,
      URLEncode.check("%23%3C%3E%5B%5D%7B%7D%7C%5E%25", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check("<", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check(">", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check("|", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check("^", URLPartUser))

    h.expect_eq[Bool](true, URLEncode.check("%23%3C", URLPartPassword))
    h.expect_eq[Bool](false, URLEncode.check("<", URLPartPassword))
    h.expect_eq[Bool](true, URLEncode.check("%23%3C", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("<", URLPartHost))
    h.expect_eq[Bool](true, URLEncode.check("%23%3C", URLPartPath))
    h.expect_eq[Bool](false, URLEncode.check("<", URLPartPath))
    h.expect_eq[Bool](true, URLEncode.check("%23%3C", URLPartQuery))
    h.expect_eq[Bool](false, URLEncode.check("<", URLPartQuery))
    h.expect_eq[Bool](true, URLEncode.check("%23%3C", URLPartFragment))
    h.expect_eq[Bool](false, URLEncode.check("<", URLPartFragment))

    // Delimiters, whether need to be encoded depends on URL part.
    h.expect_eq[Bool](true, URLEncode.check("%3A%40%2F%3F", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check(":", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check("@", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check("/", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check("?", URLPartUser))
    h.expect_eq[Bool](true, URLEncode.check(":%40%2F%3F", URLPartPassword))
    h.expect_eq[Bool](false, URLEncode.check("@", URLPartPassword))
    h.expect_eq[Bool](false, URLEncode.check("/", URLPartPassword))
    h.expect_eq[Bool](false, URLEncode.check("?", URLPartPassword))
    h.expect_eq[Bool](true, URLEncode.check("%3A%40%2F%3F", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check(":", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("@", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("/", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("?", URLPartHost))
    h.expect_eq[Bool](true, URLEncode.check(":@/%3F", URLPartPath))
    h.expect_eq[Bool](false, URLEncode.check("?", URLPartPath))
    h.expect_eq[Bool](true, URLEncode.check(":@/?", URLPartQuery))
    h.expect_eq[Bool](true, URLEncode.check(":@/?", URLPartFragment))

    true
    
class iso _CheckBad is UnitTest
  fun name(): String => "net/http/URLEncode.check_bad"

  fun apply(h: TestHelper): TestResult =>
    h.expect_eq[Bool](false, URLEncode.check("%2G", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check("%xx", URLPartUser))
    h.expect_eq[Bool](false, URLEncode.check("%2", URLPartUser))
    true

class iso _CheckScheme is UnitTest
  fun name(): String => "net/http/URLEncode.check_scheme"

  fun apply(h: TestHelper): TestResult =>
    h.expect_eq[Bool](true, URLEncode.check_scheme("Aa4-+."))
    h.expect_eq[Bool](false, URLEncode.check_scheme("_"))
    h.expect_eq[Bool](false, URLEncode.check_scheme(":"))
    h.expect_eq[Bool](false, URLEncode.check_scheme("%41"))
    true
    
class iso _CheckIPv6 is UnitTest
  fun name(): String => "net/http/URLEncode.check_ipv6"

  fun apply(h: TestHelper): TestResult =>
    // Allowed hex digits, '.' and ':' only, between '[' and ']'.
    h.expect_eq[Bool](true, URLEncode.check("[1::A.B]", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("[G]", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("[/]", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("[%32]", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("[1]2", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("[1", URLPartHost))
    h.expect_eq[Bool](false, URLEncode.check("1]", URLPartHost))
    true
    
class iso _Decode is UnitTest
  fun name(): String => "net/http/URLEncode.decode"

  fun apply(h: TestHelper): TestResult ? =>
    h.expect_eq[String]("Aa4-._~Aa4-._~",
      URLEncode.decode("Aa4-._~%41%61%34%2D%2E%5F%7E"))

    h.expect_eq[String]("F_12x", URLEncode.decode("F_1%32x"))

    h.expect_eq[String]("!$&'()*+,;=!$&'()*+,;=",
      URLEncode.decode("!$&'()*+,;=%21%24%26%27%28%29%2A%2B%2C%3B%3D"))

    h.expect_eq[String]("#<>[]{}|^ #<>[]{}|^ %",
      URLEncode.decode("#<>[]{}|^ %23%3C%3E%5B%5D%7B%7D%7C%5E%20%25"))

    true
    
class iso _DecodeBad is UnitTest
  fun name(): String => "net/http/URLEncode.decode_bad"

  fun apply(h: TestHelper): TestResult =>
    h.expect_error(lambda()? => URLEncode.decode("%2G") end)
    h.expect_error(lambda()? => URLEncode.decode("%xx") end)
    h.expect_error(lambda()? => URLEncode.decode("%2") end)
    true


class iso _BuildBasic is UnitTest
  fun name(): String => "net/http/URL.build_basic"

  fun apply(h: TestHelper): TestResult ? =>
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
      "http", "user", "", "host", 80, "", "", "")

    true
    
class iso _BuildMissingParts is UnitTest
  fun name(): String => "net/http/URL.build_missing_parts"

  fun apply(h: TestHelper): TestResult ? =>
    _Test(h, URL.build("https://user@host.name/path#fragment"),
      "https", "user", "", "host.name", 443, "/path", "", "fragment")
      
    _Test(h, URL.build("https://user@host.name#fragment"),
      "https", "user", "", "host.name", 443, "", "", "fragment")
      
    _Test(h, URL.build("//host.name/path"),
      "", "", "", "host.name", 0, "/path", "", "")
      
    _Test(h, URL.build("/path"),
      "", "", "", "", 0, "/path", "", "")
      
    _Test(h, URL.build("?query"),
      "", "", "", "", 0, "", "query", "")
      
    _Test(h, URL.build("#fragment"),
      "", "", "", "", 0, "", "", "fragment")
      
    _Test(h, URL.build("https://host.name/path#frag?ment"),
      "https", "", "", "host.name", 443, "/path", "", "frag?ment")
      
    _Test(h, URL.build("https://user@host.name?quer/y#fragment"),
      "https", "user", "", "host.name", 443, "", "quer/y", "fragment")

    true
    
class iso _BuildBad is UnitTest
  fun name(): String => "net/http/URL.build_bad"

  fun apply(h: TestHelper): TestResult =>
    h.expect_error(lambda()? =>
      URL.build("htt_ps://user@host.name/path#fragment") end)
    
    h.expect_error(lambda()? =>
      URL.build("https://[11::24_]/path") end)
    
    h.expect_error(lambda()? =>
      URL.build("https://[11::24/path") end)
    
    h.expect_error(lambda()? =>
      URL.build("https://host%2Gname/path") end)
    
    h.expect_error(lambda()? =>
      URL.build("https://hostname/path%") end)
    
    true

class iso _BuildNoEncoding is UnitTest
  fun name(): String => "net/http/URL.build_no_encoding"

  fun apply(h: TestHelper): TestResult ? =>
    _Test(h, URL.build("https://host.name/path%32path", false),
      "https", "", "", "host.name", 443, "/path%2532path", "", "")
      
    true


class iso _Valid is UnitTest
  fun name(): String => "net/http/URL.valid"

  fun apply(h: TestHelper): TestResult ? =>
    _Test(h,
      URL.valid("https://user:password@host.name:12345/path?query#fragment"),
      "https", "user", "password", "host.name", 12345, "/path", "query",
      "fragment")
    
    h.expect_error(lambda()? =>
      URL.valid("http://rosettacode.org/wiki/Category[Pony]") end)
    
    h.expect_error(lambda()? =>
      URL.valid("https://en.wikipedia|org/wiki/Polymorphism_" +
        "(computer_science)#Parametric_polymorphism") end)

    _Test(h, URL.valid("http://user@host"),
      "http", "user", "", "host", 80, "", "", "")

    true
    

class iso _ToStringFun is UnitTest
  fun name(): String => "net/http/URL.to_string"

  fun apply(h: TestHelper): TestResult ? =>
    h.expect_eq[String](
      "https://user:password@host.name:12345/path?query#fragment",
      URL.build("https://user:password@host.name:12345/path?query#fragment")
        .string())
    
    h.expect_eq[String]("http://rosettacode.org/wiki/Category%5DPony",
      URL.build("http://rosettacode.org/wiki/Category]Pony").string())

    h.expect_eq[String]("http://user@host",
      URL.build("http://user@host").string())

    // Default ports should be omitted.
    h.expect_eq[String]("http://host.name/path",
      URL.build("http://host.name:80/path").string())
    
    true


primitive _Test
  fun apply(h: TestHelper, url: URL, scheme: String, user: String,
    password: String, host: String, port: U16, path: String,
    query: String, fragment: String)
  =>
    h.expect_eq[String](scheme, url.scheme)
    h.expect_eq[String](user, url.user)
    h.expect_eq[String](password, url.password)
    h.expect_eq[String](host, url.host)
    h.expect_eq[U16](port, url.port)
    h.expect_eq[String](path, url.path)
    h.expect_eq[String](query, url.query)
    h.expect_eq[String](fragment, url.fragment)
