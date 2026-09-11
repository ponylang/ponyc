use "pony_test"
use "pony_check"

class \nodoc\ iso _TestBuilderSimpleExpansion is UnitTest
  """String variables produce correct expansion via builder."""
  fun name(): String => "uri/template/builder: simple expansion"

  fun apply(h: TestHelper) =>
    try
      let result = URITemplateBuilder("{scheme}://{host}/users/{user}")
        .set("scheme", "https")
        .set("host", "example.com")
        .set("user", "fred")
        .build()?
      h.assert_eq[String val](
        "https://example.com/users/fred", consume result)
    else
      h.fail("build should not fail")
    end

class \nodoc\ iso _TestBuilderListAndPairs is UnitTest
  """set_list and set_pairs produce correct expansion via builder."""
  fun name(): String => "uri/template/builder: list and pairs"

  fun apply(h: TestHelper) =>
    try
      let result = URITemplateBuilder("https://example.com{/path*}{?query*}")
        .set_list(
          "path",
          recover val ["api"; "v1"; "users"] end)
        .set_pairs(
          "query",
          recover val [("page", "1"); ("limit", "10")] end)
        .build()?
      h.assert_eq[String val](
        "https://example.com/api/v1/users?page=1&limit=10", consume result)
    else
      h.fail("build should not fail")
    end

class \nodoc\ iso _TestBuilderInvalidTemplate is UnitTest
  """build() errors on invalid template syntax."""
  fun name(): String => "uri/template/builder: invalid template"

  fun apply(h: TestHelper) =>
    try
      URITemplateBuilder("{=invalid}")
        .set("x", "value")
        .build()?
      h.fail("build should fail for invalid template")
    end

class \nodoc\ iso _TestBuilderEmptyVars is UnitTest
  """No variables set — undefined expansion per RFC 6570."""
  fun name(): String => "uri/template/builder: empty vars"

  fun apply(h: TestHelper) =>
    try
      let result = URITemplateBuilder("{?x,y,z}").build()?
      h.assert_eq[String val]("", consume result)
    else
      h.fail("build should not fail")
    end

class \nodoc\ iso _TestBuilderChaining is UnitTest
  """Multi-method chain produces correct result."""
  fun name(): String => "uri/template/builder: chaining"

  fun apply(h: TestHelper) =>
    try
      let result = URITemplateBuilder("{scheme}://{host}{/path*}{?query*}")
        .set("scheme", "https")
        .set("host", "example.com")
        .set_list(
          "path",
          recover val ["api"; "v1"; "users"] end)
        .set_pairs(
          "query",
          recover val [("page", "1"); ("limit", "10")] end)
        .build()?
      h.assert_eq[String val](
        "https://example.com/api/v1/users?page=1&limit=10", consume result)
    else
      h.fail("build should not fail")
    end

class \nodoc\ iso _TestPropertyBuilderMatchesExpand is Property1[String]
  """
  Property: builder with .set("x", value).build() on template "{x}" produces
  the same result as URITemplate("{x}")?.expand(vars) for any unreserved value.
  """
  fun name(): String =>
    "uri/template/builder/property: matches direct expand"

  fun gen(): Generator[String] =>
    _TemplateGenerators.unreserved_string(1, 30)

  fun ref property(arg1: String, h: PropertyHelper) =>
    // Direct path: parse + expand
    let vars = URITemplateVariables
    vars.set("x", arg1)
    let direct: String val =
      try
        let r: String val = URITemplate("{x}")?.expand(vars)
        r
      else
        h.fail("failed to parse {x}")
        return
      end

    // Builder path
    let built: String val =
      try
        let r: String val = URITemplateBuilder("{x}")
          .set("x", arg1)
          .build()?
        r
      else
        h.fail("builder should not fail for valid template")
        return
      end

    h.assert_eq[String val](direct, built)

class \nodoc\ iso _TestPropertyBuilderInvalidFails is Property1[String]
  """Property: for any invalid template, build() errors."""
  fun name(): String =>
    "uri/template/builder/property: invalid templates fail"

  fun gen(): Generator[String] =>
    _TemplateGenerators.invalid_template()

  fun ref property(arg1: String, h: PropertyHelper) =>
    try
      URITemplateBuilder(arg1).build()?
      h.fail("invalid template should not build: '" + arg1 + "'")
    end
