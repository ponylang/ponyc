use "pony_test"
use doc = ".."

class \nodoc\ _TestHtmlEscapeContent is UnitTest
  fun name(): String => "HtmlEscape/content"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.HtmlEscape.content("hello"),
      "hello")
    h.assert_eq[String](
      doc.HtmlEscape.content("<div>"),
      "&lt;div&gt;")
    h.assert_eq[String](
      doc.HtmlEscape.content("a & b"),
      "a &amp; b")
    h.assert_eq[String](
      doc.HtmlEscape.content(""),
      "")

class \nodoc\ _TestHtmlEscapeContentNoDoubleEscape is UnitTest
  fun name(): String => "HtmlEscape/content-no-double-escape"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.HtmlEscape.content("&amp;"),
      "&amp;amp;")

class \nodoc\ _TestHtmlEscapeAttribute is UnitTest
  fun name(): String => "HtmlEscape/attribute"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.HtmlEscape.attribute("hello"),
      "hello")
    h.assert_eq[String](
      doc.HtmlEscape.attribute("a\"b"),
      "a&quot;b")
    h.assert_eq[String](
      doc.HtmlEscape.attribute("<a&b>"),
      "&lt;a&amp;b&gt;")
    h.assert_eq[String](
      doc.HtmlEscape.attribute("it's"),
      "it's")

class \nodoc\ _TestHtmlLinkFormatMethods is UnitTest
  fun name(): String => "HtmlLinkFormat/methods"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.HtmlLinkFormat.link("Array", "builtin-Array"),
      "<a href=\"builtin-Array.html\">Array</a>")
    h.assert_eq[String](
      doc.HtmlLinkFormat.open_bracket(),
      "[")
    h.assert_eq[String](
      doc.HtmlLinkFormat.close_bracket(),
      "]")

class \nodoc\ _TestHtmlLinkFormatEscaping is UnitTest
  fun name(): String => "HtmlLinkFormat/escaping"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.HtmlLinkFormat.link("A<B>", "pkg-A\"B"),
      "<a href=\"pkg-A&quot;B.html\">A&lt;B&gt;</a>")

class \nodoc\ _TestHtmlLinkFormatTypeRenderer is UnitTest
  fun name(): String => "HtmlLinkFormat/type-renderer"

  fun apply(h: TestHelper) =>
    let string_t =
      doc.DocNominal(
        "String",
        "builtin-String",
        recover val Array[doc.DocType] end,
        None,
        None,
        false,
        false)
    let type_args: Array[doc.DocType] val =
      recover val [as doc.DocType: string_t] end
    let t =
      doc.DocNominal(
        "Array",
        "builtin-Array",
        type_args,
        None,
        None,
        false,
        false)

    h.assert_eq[String](
      doc.TypeRenderer.render(t, doc.HtmlLinkFormat, false, false),
      "<a href=\"builtin-Array.html\">Array</a>" +
        "[<a href=\"builtin-String.html\">String</a>]")
