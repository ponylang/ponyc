use "pony_test"

primitive \nodoc\ _RFC6570Vars
  """
  Build the RFC 6570 standard variable set used across test groups.
  """
  fun apply(): URITemplateVariables =>
    let vars = URITemplateVariables
    vars .> set("count", "one,two,three")
      .> set("dom", "example.com")
      .> set("dub", "me/too")
      .> set("hello", "Hello World!")
      .> set("half", "50%")
      .> set("var", "value")
      .> set("who", "fred")
      .> set("base", "http://example.com/home/")
      .> set("path", "/foo/bar")
      .> set("v", "6")
      .> set("x", "1024")
      .> set("y", "768")
      .> set("empty", "")
      .> set_list("list", recover val ["red"; "green"; "blue"] end)
      .> set_pairs(
        "keys",
        recover val [("semi", ";"); ("dot", "."); ("comma", ",")] end)
    vars

class \nodoc\ iso _TestSimpleExpansion is UnitTest
  """RFC 6570 Section 3.2.2: Simple string expansion."""
  fun name(): String => "uri/template/simple expansion"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    _assert_expand(h, "{var}", "value", vars)
    _assert_expand(h, "{hello}", "Hello%20World%21", vars)
    _assert_expand(h, "{half}", "50%25", vars)
    _assert_expand(h, "O{empty}X", "OX", vars)
    _assert_expand(h, "O{undef}X", "OX", vars)
    _assert_expand(h, "{x,y}", "1024,768", vars)
    _assert_expand(h, "{x,hello,y}", "1024,Hello%20World%21,768", vars)
    _assert_expand(h, "?{x,empty}", "?1024,", vars)
    _assert_expand(h, "?{x,undef}", "?1024", vars)
    _assert_expand(h, "?{undef,y}", "?768", vars)
    _assert_expand(h, "{var:3}", "val", vars)
    _assert_expand(h, "{var:30}", "value", vars)
    _assert_expand(h, "{list}", "red,green,blue", vars)
    _assert_expand(h, "{list*}", "red,green,blue", vars)
    _assert_expand(h, "{keys}", "semi,%3B,dot,.,comma,%2C", vars)
    _assert_expand(h, "{keys*}", "semi=%3B,dot=.,comma=%2C", vars)

  fun _assert_expand(
    h: TestHelper,
    template: String,
    expected: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](
        expected, result where msg = "template: " + template)
    else
      h.fail("failed to parse template: " + template)
    end

class \nodoc\ iso _TestReservedExpansion is UnitTest
  """RFC 6570 Section 3.2.3: Reserved expansion (+)."""
  fun name(): String => "uri/template/reserved expansion"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    _assert_expand(h, "{+var}", "value", vars)
    _assert_expand(h, "{+hello}", "Hello%20World!", vars)
    _assert_expand(h, "{+half}", "50%25", vars)
    _assert_expand(
      h,
      "{base}index",
      "http%3A%2F%2Fexample.com%2Fhome%2Findex",
      vars)
    _assert_expand(h, "{+base}index", "http://example.com/home/index", vars)
    _assert_expand(h, "O{+empty}X", "OX", vars)
    _assert_expand(h, "O{+undef}X", "OX", vars)
    _assert_expand(h, "{+path}/here", "/foo/bar/here", vars)
    _assert_expand(h, "here?ref={+path}", "here?ref=/foo/bar", vars)
    _assert_expand(
      h,
      "up{+path}{var}/here",
      "up/foo/barvalue/here",
      vars)
    _assert_expand(h, "{+x,hello,y}", "1024,Hello%20World!,768", vars)
    _assert_expand(h, "{+path,x}/here", "/foo/bar,1024/here", vars)
    _assert_expand(h, "{+path:6}/here", "/foo/b/here", vars)
    _assert_expand(h, "{+list}", "red,green,blue", vars)
    _assert_expand(h, "{+list*}", "red,green,blue", vars)
    _assert_expand(h, "{+keys}", "semi,;,dot,.,comma,,", vars)
    _assert_expand(h, "{+keys*}", "semi=;,dot=.,comma=,", vars)

  fun _assert_expand(
    h: TestHelper,
    template: String,
    expected: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](
        expected, result where msg = "template: " + template)
    else
      h.fail("failed to parse template: " + template)
    end

class \nodoc\ iso _TestFragmentExpansion is UnitTest
  """RFC 6570 Section 3.2.4: Fragment expansion (#)."""
  fun name(): String => "uri/template/fragment expansion"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    _assert_expand(h, "{#var}", "#value", vars)
    _assert_expand(h, "{#hello}", "#Hello%20World!", vars)
    _assert_expand(h, "{#half}", "#50%25", vars)
    _assert_expand(h, "foo{#empty}", "foo#", vars)
    _assert_expand(h, "foo{#undef}", "foo", vars)
    _assert_expand(h, "{#x,hello,y}", "#1024,Hello%20World!,768", vars)
    _assert_expand(h, "{#path,x}/here", "#/foo/bar,1024/here", vars)
    _assert_expand(h, "{#path:6}/here", "#/foo/b/here", vars)
    _assert_expand(h, "{#list}", "#red,green,blue", vars)
    _assert_expand(h, "{#list*}", "#red,green,blue", vars)
    _assert_expand(h, "{#keys}", "#semi,;,dot,.,comma,,", vars)
    _assert_expand(h, "{#keys*}", "#semi=;,dot=.,comma=,", vars)

  fun _assert_expand(
    h: TestHelper,
    template: String,
    expected: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](
        expected, result where msg = "template: " + template)
    else
      h.fail("failed to parse template: " + template)
    end

class \nodoc\ iso _TestLabelExpansion is UnitTest
  """RFC 6570 Section 3.2.5: Label expansion with dot-prefix (.)."""
  fun name(): String => "uri/template/label expansion"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    _assert_expand(h, "{.who}", ".fred", vars)
    _assert_expand(h, "{.who,who}", ".fred.fred", vars)
    _assert_expand(h, "{.half,who}", ".50%25.fred", vars)
    _assert_expand(h, "X{.var}", "X.value", vars)
    _assert_expand(h, "X{.empty}", "X.", vars)
    _assert_expand(h, "X{.undef}", "X", vars)
    _assert_expand(h, "X{.var:3}", "X.val", vars)
    _assert_expand(h, "X{.list}", "X.red,green,blue", vars)
    _assert_expand(h, "X{.list*}", "X.red.green.blue", vars)
    _assert_expand(h, "X{.keys}", "X.semi,%3B,dot,.,comma,%2C", vars)
    _assert_expand(h, "X{.keys*}", "X.semi=%3B.dot=..comma=%2C", vars)
    _assert_expand(h, "X{.empty,who}", "X..fred", vars)

  fun _assert_expand(
    h: TestHelper,
    template: String,
    expected: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](
        expected, result where msg = "template: " + template)
    else
      h.fail("failed to parse template: " + template)
    end

class \nodoc\ iso _TestPathSegmentExpansion is UnitTest
  """RFC 6570 Section 3.2.6: Path segments (/)."""
  fun name(): String => "uri/template/path segment expansion"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    _assert_expand(h, "{/who}", "/fred", vars)
    _assert_expand(h, "{/who,who}", "/fred/fred", vars)
    _assert_expand(h, "{/half,who}", "/50%25/fred", vars)
    _assert_expand(h, "{/who,dub}", "/fred/me%2Ftoo", vars)
    _assert_expand(h, "{/var}", "/value", vars)
    _assert_expand(h, "{/var,empty}", "/value/", vars)
    _assert_expand(h, "{/var,undef}", "/value", vars)
    _assert_expand(h, "{/var,x}/here", "/value/1024/here", vars)
    _assert_expand(h, "{/var:1,var}", "/v/value", vars)
    _assert_expand(h, "{/list}", "/red,green,blue", vars)
    _assert_expand(h, "{/list*}", "/red/green/blue", vars)
    _assert_expand(h, "{/list*,path:4}", "/red/green/blue/%2Ffoo", vars)
    _assert_expand(h, "{/keys}", "/semi,%3B,dot,.,comma,%2C", vars)
    _assert_expand(h, "{/keys*}", "/semi=%3B/dot=./comma=%2C", vars)

  fun _assert_expand(
    h: TestHelper,
    template: String,
    expected: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](
        expected, result where msg = "template: " + template)
    else
      h.fail("failed to parse template: " + template)
    end

class \nodoc\ iso _TestPathParameterExpansion is UnitTest
  """RFC 6570 Section 3.2.7: Path-style parameters (;)."""
  fun name(): String => "uri/template/path parameter expansion"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    _assert_expand(h, "{;who}", ";who=fred", vars)
    _assert_expand(h, "{;half}", ";half=50%25", vars)
    _assert_expand(h, "{;empty}", ";empty", vars)
    _assert_expand(h, "{;v,empty,who}", ";v=6;empty;who=fred", vars)
    _assert_expand(h, "{;v,bar,who}", ";v=6;who=fred", vars)
    _assert_expand(h, "{;x,y}", ";x=1024;y=768", vars)
    _assert_expand(h, "{;x,y,empty}", ";x=1024;y=768;empty", vars)
    _assert_expand(h, "{;x,y,undef}", ";x=1024;y=768", vars)
    _assert_expand(h, "{;hello:5}", ";hello=Hello", vars)
    _assert_expand(h, "{;list}", ";list=red,green,blue", vars)
    _assert_expand(h, "{;list*}", ";list=red;list=green;list=blue", vars)
    _assert_expand(h, "{;keys}", ";keys=semi,%3B,dot,.,comma,%2C", vars)
    _assert_expand(h, "{;keys*}", ";semi=%3B;dot=.;comma=%2C", vars)

  fun _assert_expand(
    h: TestHelper,
    template: String,
    expected: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](
        expected, result where msg = "template: " + template)
    else
      h.fail("failed to parse template: " + template)
    end

class \nodoc\ iso _TestQueryExpansion is UnitTest
  """RFC 6570 Section 3.2.8: Form-style query (?)."""
  fun name(): String => "uri/template/query expansion"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    _assert_expand(h, "{?who}", "?who=fred", vars)
    _assert_expand(h, "{?half}", "?half=50%25", vars)
    _assert_expand(h, "{?x,y}", "?x=1024&y=768", vars)
    _assert_expand(h, "{?x,y,empty}", "?x=1024&y=768&empty=", vars)
    _assert_expand(h, "{?x,y,undef}", "?x=1024&y=768", vars)
    _assert_expand(h, "{?var:3}", "?var=val", vars)
    _assert_expand(h, "{?list}", "?list=red,green,blue", vars)
    _assert_expand(h, "{?list*}", "?list=red&list=green&list=blue", vars)
    _assert_expand(h, "{?keys}", "?keys=semi,%3B,dot,.,comma,%2C", vars)
    _assert_expand(h, "{?keys*}", "?semi=%3B&dot=.&comma=%2C", vars)

  fun _assert_expand(
    h: TestHelper,
    template: String,
    expected: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](
        expected, result where msg = "template: " + template)
    else
      h.fail("failed to parse template: " + template)
    end

class \nodoc\ iso _TestQueryContinuationExpansion is UnitTest
  """RFC 6570 Section 3.2.9: Form-style query continuation (&)."""
  fun name(): String => "uri/template/query continuation expansion"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    _assert_expand(h, "{&who}", "&who=fred", vars)
    _assert_expand(h, "{&half}", "&half=50%25", vars)
    _assert_expand(h, "?fixed=yes{&x}", "?fixed=yes&x=1024", vars)
    _assert_expand(h, "{&x,y,empty}", "&x=1024&y=768&empty=", vars)
    _assert_expand(h, "{&x,y,undef}", "&x=1024&y=768", vars)
    _assert_expand(h, "{&var:3}", "&var=val", vars)
    _assert_expand(h, "{&list}", "&list=red,green,blue", vars)
    _assert_expand(h, "{&list*}", "&list=red&list=green&list=blue", vars)
    _assert_expand(h, "{&keys}", "&keys=semi,%3B,dot,.,comma,%2C", vars)
    _assert_expand(h, "{&keys*}", "&semi=%3B&dot=.&comma=%2C", vars)

  fun _assert_expand(
    h: TestHelper,
    template: String,
    expected: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](
        expected, result where msg = "template: " + template)
    else
      h.fail("failed to parse template: " + template)
    end

class \nodoc\ iso _TestEmptyListUndefined is UnitTest
  """Empty list is treated as undefined — produces no output."""
  fun name(): String => "uri/template/empty list is undefined"

  fun apply(h: TestHelper) =>
    let vars = URITemplateVariables
    vars.set_list(
      "empty_list", recover val Array[String val] end)
    vars.set("x", "1024")

    try
      let tpl = URITemplate("{?x,empty_list}")?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val]("?x=1024", result)
    else
      h.fail("failed to parse template")
    end

class \nodoc\ iso _TestEmptyPairsUndefined is UnitTest
  """Empty pairs is treated as undefined — produces no output."""
  fun name(): String => "uri/template/empty pairs is undefined"

  fun apply(h: TestHelper) =>
    let vars = URITemplateVariables
    vars.set_pairs(
      "empty_pairs",
      recover val Array[(String val, String val)] end)
    vars.set("x", "1024")

    try
      let tpl = URITemplate("{?x,empty_pairs}")?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val]("?x=1024", result)
    else
      h.fail("failed to parse template")
    end

class \nodoc\ iso _TestAllUndefined is UnitTest
  """All variables undefined produces empty string for expression."""
  fun name(): String => "uri/template/all undefined"

  fun apply(h: TestHelper) =>
    let vars = URITemplateVariables

    try
      let tpl = URITemplate("{?x,y,z}")?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val]("", result)
    else
      h.fail("failed to parse template")
    end

class \nodoc\ iso _TestExplodeListQuery is UnitTest
  """Exploded list with query: each item gets name= prefix."""
  fun name(): String => "uri/template/explode list query"

  fun apply(h: TestHelper) =>
    let vars = URITemplateVariables
    vars.set_list(
      "colors", recover val ["red"; "green"; "blue"] end)

    try
      let tpl = URITemplate("{?colors*}")?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val]("?colors=red&colors=green&colors=blue", result)
    else
      h.fail("failed to parse template")
    end

class \nodoc\ iso _TestExplodePairsQuery is UnitTest
  """Exploded pairs with query: each pair becomes key=value."""
  fun name(): String => "uri/template/explode pairs query"

  fun apply(h: TestHelper) =>
    let vars = URITemplateVariables
    vars.set_pairs(
      "opts",
      recover val [("page", "1"); ("size", "10")] end)

    try
      let tpl = URITemplate("{?opts*}")?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val]("?page=1&size=10", result)
    else
      h.fail("failed to parse template")
    end

class \nodoc\ iso _TestExplodeListSemicolon is UnitTest
  """Exploded list with semicolon: name=value for each item per RFC 6570."""
  fun name(): String => "uri/template/explode list semicolon"

  fun apply(h: TestHelper) =>
    let vars = _RFC6570Vars()

    try
      let tpl = URITemplate("{;list*}")?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](";list=red;list=green;list=blue", result)
    else
      h.fail("failed to parse template")
    end

class \nodoc\ iso _TestPrefixUnicode is UnitTest
  """Prefix modifier counts codepoints, not bytes."""
  fun name(): String => "uri/template/prefix counts codepoints"

  fun apply(h: TestHelper) =>
    let vars = URITemplateVariables
    // Build "cafés" manually: c a f é(0xC3 0xA9) s
    let word =
      recover val
        String
          .> push('c')
          .> push('a')
          .> push('f')
          .> push(0xC3)
          .> push(0xA9)
          .> push('s')
      end
    vars.set("word", word)

    try
      let tpl = URITemplate("{word:4}")?
      let result: String val = tpl.expand(vars)
      // First 4 codepoints: c a f é => encoded as caf%C3%A9
      h.assert_eq[String val]("caf%C3%A9", result)
    else
      h.fail("failed to parse template")
    end

class \nodoc\ iso _TestTemplateString is UnitTest
  """URITemplate.string() returns the original template."""
  fun name(): String => "uri/template/string roundtrip"

  fun apply(h: TestHelper) =>
    let templates: Array[String] val =
      [
        ""
        "literal"
        "{var}"
        "{+path}/here{?x,y}"
      ]

    for template in templates.values() do
      try
        let tpl = URITemplate(template)?
        h.assert_eq[String val](template, tpl.string())
      else
        h.fail("failed to parse: " + template)
      end
    end
