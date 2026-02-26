use "pony_test"
use "pony_check"
use lint = ".."

// --- is_camel_case ---

class \nodoc\ _TestIsCamelCaseValid is UnitTest
  """Valid CamelCase names pass."""
  fun name(): String => "NamingHelpers: is_camel_case valid examples"

  fun apply(h: TestHelper) =>
    h.assert_true(lint.NamingHelpers.is_camel_case("Foo"))
    h.assert_true(lint.NamingHelpers.is_camel_case("FooBar"))
    h.assert_true(lint.NamingHelpers.is_camel_case("F"))
    h.assert_true(lint.NamingHelpers.is_camel_case("HTTPClient"))
    h.assert_true(lint.NamingHelpers.is_camel_case("A1B2"))
    h.assert_true(lint.NamingHelpers.is_camel_case("_PrivateType"))
    h.assert_true(lint.NamingHelpers.is_camel_case("_F"))
    // Trailing prime is accepted
    h.assert_true(lint.NamingHelpers.is_camel_case("Foo'"))
    h.assert_true(lint.NamingHelpers.is_camel_case("_Foo'"))

class \nodoc\ _TestIsCamelCaseInvalid is UnitTest
  """Invalid CamelCase names fail."""
  fun name(): String => "NamingHelpers: is_camel_case invalid examples"

  fun apply(h: TestHelper) =>
    h.assert_false(lint.NamingHelpers.is_camel_case("foo"))
    h.assert_false(lint.NamingHelpers.is_camel_case("foo_bar"))
    h.assert_false(lint.NamingHelpers.is_camel_case("Foo_Bar"))
    h.assert_false(lint.NamingHelpers.is_camel_case(""))
    h.assert_false(lint.NamingHelpers.is_camel_case("_"))
    h.assert_false(lint.NamingHelpers.is_camel_case("_foo"))
    h.assert_false(lint.NamingHelpers.is_camel_case("123"))
    // Prime alone is not valid
    h.assert_false(lint.NamingHelpers.is_camel_case("'"))

// --- is_snake_case ---
class \nodoc\ _TestIsSnakeCaseValid is UnitTest
  """Valid snake_case names pass."""
  fun name(): String => "NamingHelpers: is_snake_case valid examples"

  fun apply(h: TestHelper) =>
    h.assert_true(lint.NamingHelpers.is_snake_case("foo"))
    h.assert_true(lint.NamingHelpers.is_snake_case("foo_bar"))
    h.assert_true(lint.NamingHelpers.is_snake_case("f"))
    h.assert_true(lint.NamingHelpers.is_snake_case("a1"))
    h.assert_true(lint.NamingHelpers.is_snake_case("my_fun_2"))
    h.assert_true(lint.NamingHelpers.is_snake_case("_private"))
    h.assert_true(lint.NamingHelpers.is_snake_case("_f"))
    h.assert_true(lint.NamingHelpers.is_snake_case("create"))
    // Trailing prime is accepted
    h.assert_true(lint.NamingHelpers.is_snake_case("foo'"))
    h.assert_true(lint.NamingHelpers.is_snake_case("rule_id'"))
    h.assert_true(lint.NamingHelpers.is_snake_case("_private'"))

class \nodoc\ _TestIsSnakeCaseInvalid is UnitTest
  """Invalid snake_case names fail."""
  fun name(): String => "NamingHelpers: is_snake_case invalid examples"

  fun apply(h: TestHelper) =>
    h.assert_false(lint.NamingHelpers.is_snake_case("Foo"))
    h.assert_false(lint.NamingHelpers.is_snake_case("FooBar"))
    h.assert_false(lint.NamingHelpers.is_snake_case("foo_Bar"))
    h.assert_false(lint.NamingHelpers.is_snake_case(""))
    h.assert_false(lint.NamingHelpers.is_snake_case("_"))
    h.assert_false(lint.NamingHelpers.is_snake_case("_Foo"))
    h.assert_false(lint.NamingHelpers.is_snake_case("myFun"))
    // Prime alone is not valid
    h.assert_false(lint.NamingHelpers.is_snake_case("'"))

// --- has_lowered_acronym ---
class \nodoc\ _TestHasLoweredAcronym is UnitTest
  """Detects lowered acronyms in CamelCase names."""
  fun name(): String => "NamingHelpers: has_lowered_acronym"

  fun apply(h: TestHelper) =>
    // Lowered form should be detected
    h.assert_eq[String]("JSON",
      try lint.NamingHelpers.has_lowered_acronym("JsonDoc") as String
      else "" end)
    h.assert_eq[String]("HTTP",
      try lint.NamingHelpers.has_lowered_acronym("HttpClient") as String
      else "" end)
    h.assert_eq[String]("URL",
      try lint.NamingHelpers.has_lowered_acronym("UrlParser") as String
      else "" end)
    h.assert_eq[String]("TCP",
      try lint.NamingHelpers.has_lowered_acronym("TcpConnection") as String
      else "" end)
    // Properly uppercased should NOT be detected
    h.assert_true(
      lint.NamingHelpers.has_lowered_acronym("JSONDoc") is None)
    h.assert_true(
      lint.NamingHelpers.has_lowered_acronym("HTTPClient") is None)
    h.assert_true(
      lint.NamingHelpers.has_lowered_acronym("MyClass") is None)

// --- to_snake_case ---
class \nodoc\ _TestToSnakeCase is UnitTest
  """CamelCase to snake_case conversion, including initialisms."""
  fun name(): String => "NamingHelpers: to_snake_case"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("ssl_context",
      lint.NamingHelpers.to_snake_case("SSLContext"))
    h.assert_eq[String]("url_encode",
      lint.NamingHelpers.to_snake_case("URLEncode"))
    h.assert_eq[String]("contents_log",
      lint.NamingHelpers.to_snake_case("ContentsLog"))
    h.assert_eq[String]("_client_connection",
      lint.NamingHelpers.to_snake_case("_ClientConnection"))
    h.assert_eq[String]("foo",
      lint.NamingHelpers.to_snake_case("Foo"))
    h.assert_eq[String]("foo_bar",
      lint.NamingHelpers.to_snake_case("FooBar"))
    h.assert_eq[String]("http_client",
      lint.NamingHelpers.to_snake_case("HTTPClient"))
    h.assert_eq[String]("json_parser",
      lint.NamingHelpers.to_snake_case("JSONParser"))
    h.assert_eq[String]("a",
      lint.NamingHelpers.to_snake_case("A"))
    h.assert_eq[String]("_a",
      lint.NamingHelpers.to_snake_case("_A"))

// --- Property tests ---
class \nodoc\ _TestCamelCaseValidProperty is Property1[String]
  """Property: generated valid CamelCase always passes is_camel_case."""

  fun name(): String =>
    "NamingHelpers: valid CamelCase property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness)
          : GenerateResult[String]
        =>
          let len = rnd.u8(1, 10).usize()
          let s = recover iso String(len + 2) end
          // Optional leading underscore (~25%)
          if rnd.bool() and rnd.bool() then
            s.push('_')
          end
          // First significant char: uppercase
          s.push('A' + rnd.u8(0, 25))
          // Remaining: alphanumeric only
          var i: USize = 1
          while i < len do
            let ch = rnd.u8(0, 61)
            if ch < 26 then
              s.push('A' + ch)
            elseif ch < 52 then
              s.push('a' + (ch - 26))
            else
              s.push('0' + (ch - 52))
            end
            i = i + 1
          end
          consume s
      end)

  fun ref property(arg1: String, ph: PropertyHelper) =>
    let n: String val = arg1.clone()
    ph.assert_true(
      lint.NamingHelpers.is_camel_case(n),
      "Expected valid CamelCase: " + n)

class \nodoc\ _TestCamelCaseInvalidProperty is Property1[String]
  """Property: generated invalid CamelCase always fails."""

  fun name(): String =>
    "NamingHelpers: invalid CamelCase property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness)
          : GenerateResult[String]
        =>
          let mode = rnd.u8(0, 4)
          let s = recover iso String(12) end
          if mode == 0 then
            // Empty string
            None
          elseif mode == 1 then
            // Just underscore
            s.push('_')
          elseif mode == 2 then
            // Starts with lowercase
            if rnd.bool() then s.push('_') end
            s.push('a' + rnd.u8(0, 25))
            var extra = rnd.u8(0, 5).usize()
            while extra > 0 do
              s.push('a' + rnd.u8(0, 25))
              extra = extra - 1
            end
          elseif mode == 3 then
            // Underscore in body
            s.push('A' + rnd.u8(0, 25))
            s.push('a' + rnd.u8(0, 25))
            s.push('_')
            s.push('A' + rnd.u8(0, 25))
          else
            // Starts with digit
            s.push('0' + rnd.u8(0, 9))
            s.push('a' + rnd.u8(0, 25))
          end
          consume s
      end)

  fun ref property(arg1: String, ph: PropertyHelper) =>
    let n: String val = arg1.clone()
    ph.assert_false(
      lint.NamingHelpers.is_camel_case(n),
      "Expected invalid CamelCase: " + n)

class \nodoc\ _TestSnakeCaseValidProperty is Property1[String]
  """Property: generated valid snake_case always passes."""

  fun name(): String =>
    "NamingHelpers: valid snake_case property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness)
          : GenerateResult[String]
        =>
          let len = rnd.u8(1, 10).usize()
          let s = recover iso String(len + 2) end
          // Optional leading underscore (~25%)
          if rnd.bool() and rnd.bool() then
            s.push('_')
          end
          // First significant char: lowercase
          s.push('a' + rnd.u8(0, 25))
          // Remaining: lowercase, digits, underscores
          var i: USize = 1
          while i < len do
            let ch = rnd.u8(0, 36)
            if ch < 26 then
              s.push('a' + ch)
            elseif ch < 36 then
              s.push('0' + (ch - 26))
            else
              s.push('_')
            end
            i = i + 1
          end
          consume s
      end)

  fun ref property(arg1: String, ph: PropertyHelper) =>
    let n: String val = arg1.clone()
    ph.assert_true(
      lint.NamingHelpers.is_snake_case(n),
      "Expected valid snake_case: " + n)

class \nodoc\ _TestSnakeCaseInvalidProperty is Property1[String]
  """Property: generated invalid snake_case always fails."""

  fun name(): String =>
    "NamingHelpers: invalid snake_case property"

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness)
          : GenerateResult[String]
        =>
          let mode = rnd.u8(0, 3)
          let s = recover iso String(12) end
          if mode == 0 then
            // Empty string
            None
          elseif mode == 1 then
            // Just underscore
            s.push('_')
          elseif mode == 2 then
            // Starts with uppercase
            if rnd.bool() then s.push('_') end
            s.push('A' + rnd.u8(0, 25))
            var extra = rnd.u8(0, 5).usize()
            while extra > 0 do
              s.push('a' + rnd.u8(0, 25))
              extra = extra - 1
            end
          else
            // Uppercase in body
            s.push('a' + rnd.u8(0, 25))
            s.push('a' + rnd.u8(0, 25))
            s.push('A' + rnd.u8(0, 25))
            s.push('a' + rnd.u8(0, 25))
          end
          consume s
      end)

  fun ref property(arg1: String, ph: PropertyHelper) =>
    let n: String val = arg1.clone()
    ph.assert_false(
      lint.NamingHelpers.is_snake_case(n),
      "Expected invalid snake_case: " + n)

