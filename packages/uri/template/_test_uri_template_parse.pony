use "pony_test"

class \nodoc\ iso _TestParseErrorReservedOp is UnitTest
  """Reserved operators produce specific error messages."""
  fun name(): String => "uri/template/parse error: reserved operator"

  fun apply(h: TestHelper) =>
    _assert_parse_error(h, "{=var}", "reserved operator '='", 1)
    _assert_parse_error(h, "{,var}", "reserved operator ','", 1)
    _assert_parse_error(h, "{!var}", "reserved operator '!'", 1)
    _assert_parse_error(h, "{@var}", "reserved operator '@'", 1)
    _assert_parse_error(h, "{|var}", "reserved operator '|'", 1)

  fun _assert_parse_error(
    h: TestHelper,
    template: String,
    expected_msg: String,
    expected_offset: USize)
  =>
    match \exhaustive\ URITemplateParse(template)
    | let _: URITemplate =>
      h.fail("expected parse error for: " + template)
    | let err: URITemplateParseError =>
      h.assert_eq[String val](
        expected_msg, err.message
        where msg = "template: " + template)
      h.assert_eq[USize](
        expected_offset, err.offset
        where msg = "template: " + template + " offset")
    end

class \nodoc\ iso _TestParseErrorUnclosed is UnitTest
  """Unclosed expression produces error at the opening brace."""
  fun name(): String => "uri/template/parse error: unclosed"

  fun apply(h: TestHelper) =>
    match \exhaustive\ URITemplateParse("{var")
    | let _: URITemplate =>
      h.fail("expected parse error")
    | let err: URITemplateParseError =>
      h.assert_eq[String val]("unclosed expression", err.message)
      h.assert_eq[USize](0, err.offset)
    end

class \nodoc\ iso _TestParseErrorEmptyExpression is UnitTest
  """Empty expression {} produces error."""
  fun name(): String => "uri/template/parse error: empty expression"

  fun apply(h: TestHelper) =>
    match \exhaustive\ URITemplateParse("{}")
    | let _: URITemplate =>
      h.fail("expected parse error")
    | let err: URITemplateParseError =>
      h.assert_eq[String val]("empty expression", err.message)
      h.assert_eq[USize](0, err.offset)
    end

class \nodoc\ iso _TestParseErrorEmptyVarname is UnitTest
  """Trailing comma with no varname produces error."""
  fun name(): String => "uri/template/parse error: empty varname"

  fun apply(h: TestHelper) =>
    match \exhaustive\ URITemplateParse("{,}")
    | let _: URITemplate =>
      h.fail("expected parse error")
    | let err: URITemplateParseError =>
      // The comma is parsed as reserved operator
      h.assert_eq[String val]("reserved operator ','", err.message)
    end

class \nodoc\ iso _TestParseErrorPrefixBounds is UnitTest
  """Prefix values at boundaries: 0 too low, 10000 too high."""
  fun name(): String => "uri/template/parse error: prefix bounds"

  fun apply(h: TestHelper) =>
    match \exhaustive\ URITemplateParse("{var:0}")
    | let _: URITemplate =>
      h.fail("expected parse error for :0")
    | let err: URITemplateParseError =>
      h.assert_eq[String val](
        "prefix length must be at least 1", err.message)
    end

    match \exhaustive\ URITemplateParse("{var:10000}")
    | let _: URITemplate =>
      h.fail("expected parse error for :10000")
    | let err: URITemplateParseError =>
      h.assert_eq[String val](
        "prefix length exceeds 4 digits", err.message)
    end

class \nodoc\ iso _TestParseErrorDotInVarname is UnitTest
  """Leading and consecutive dots in varnames produce errors."""
  fun name(): String => "uri/template/parse error: dots in varname"

  fun apply(h: TestHelper) =>
    match \exhaustive\ URITemplateParse("{..var}")
    | let _: URITemplate =>
      h.fail("expected parse error for leading dot")
    | let err: URITemplateParseError =>
      h.assert_eq[String val]("leading dot in varname", err.message)
    end

    match \exhaustive\ URITemplateParse("{var..x}")
    | let _: URITemplate =>
      h.fail("expected parse error for consecutive dots")
    | let err: URITemplateParseError =>
      h.assert_eq[String val](
        "consecutive dots in varname", err.message)
    end

class \nodoc\ iso _TestParseErrorUnexpectedCloseBrace is UnitTest
  """Stray } in literal text produces error."""
  fun name(): String => "uri/template/parse error: unexpected }"

  fun apply(h: TestHelper) =>
    match \exhaustive\ URITemplateParse("foo}bar")
    | let _: URITemplate =>
      h.fail("expected parse error")
    | let err: URITemplateParseError =>
      h.assert_eq[String val]("unexpected '}'", err.message)
      h.assert_eq[USize](3, err.offset)
    end

class \nodoc\ iso _TestParseErrorInvalidLiteralChar is UnitTest
  """Invalid literal characters (space, <, >) produce errors."""
  fun name(): String => "uri/template/parse error: invalid literal char"

  fun apply(h: TestHelper) =>
    match \exhaustive\ URITemplateParse("foo bar")
    | let _: URITemplate =>
      h.fail("expected parse error for space")
    | let err: URITemplateParseError =>
      h.assert_eq[String val]("invalid literal character", err.message)
      h.assert_eq[USize](3, err.offset)
    end

class \nodoc\ iso _TestParseValidTemplates is UnitTest
  """Valid templates with various operators parse successfully."""
  fun name(): String => "uri/template/parse valid templates"

  fun apply(h: TestHelper) =>
    let valid: Array[String] val =
      [
        ""                          // empty template
        "literal"                   // pure literal
        "{var}"                     // simple
        "{+var}"                    // reserved
        "{#var}"                    // fragment
        "{.var}"                    // label
        "{/var}"                    // path
        "{;var}"                    // parameter
        "{?var}"                    // query
        "{&var}"                    // continuation
        "{var:3}"                   // prefix
        "{var*}"                    // explode
        "{x,y}"                     // multiple vars
        "{?x,y,z}"                  // query with multiple
        "http://example.com/{var}"  // full URL pattern
        "{var.name}"                // dot-separated varname
      ]

    for template in valid.values() do
      match \exhaustive\ URITemplateParse(template)
      | let _: URITemplate => None
      | let err: URITemplateParseError =>
        h.fail("unexpected parse error for '" + template + "': " +
          err.string())
      end
    end
