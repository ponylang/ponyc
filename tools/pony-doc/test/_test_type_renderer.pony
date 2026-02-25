use "pony_test"
use doc = ".."

class \nodoc\ _TestRenderNominalNoLink is UnitTest
  fun name(): String => "TypeRenderer/nominal-no-link"

  fun apply(h: TestHelper) =>
    let t = doc.DocNominal("Array", "builtin-Array",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    h.assert_eq[String](
      doc.TypeRenderer.render(t, false, false, false), "Array")

class \nodoc\ _TestRenderNominalWithLink is UnitTest
  fun name(): String => "TypeRenderer/nominal-with-link"

  fun apply(h: TestHelper) =>
    let t = doc.DocNominal("Array", "builtin-Array",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    h.assert_eq[String](
      doc.TypeRenderer.render(t, true, false, false),
      "[Array](builtin-Array.md)")

class \nodoc\ _TestRenderNominalWithTypeArgs is UnitTest
  fun name(): String => "TypeRenderer/nominal-type-args"

  fun apply(h: TestHelper) =>
    let string_t = doc.DocNominal("String", "builtin-String",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let type_args: Array[doc.DocType] val =
      recover val [as doc.DocType: string_t] end
    let t = doc.DocNominal("Array", "builtin-Array",
      type_args, None, None, false, false)

    // With links: escaped brackets and linked type args
    h.assert_eq[String](
      doc.TypeRenderer.render(t, true, false, false),
      "[Array](builtin-Array.md)\\[[String](builtin-String.md)\\]")

    // Without links: plain brackets
    h.assert_eq[String](
      doc.TypeRenderer.render(t, false, false, false),
      "Array[String]")

class \nodoc\ _TestRenderNominalWithCap is UnitTest
  fun name(): String => "TypeRenderer/nominal-with-cap"

  fun apply(h: TestHelper) =>
    let t = doc.DocNominal("Array", "builtin-Array",
      recover val Array[doc.DocType] end,
      "val", None, false, false)
    h.assert_eq[String](
      doc.TypeRenderer.render(t, false, false, false), "Array val")

class \nodoc\ _TestRenderNominalPrivateNoInclude is UnitTest
  fun name(): String => "TypeRenderer/nominal-private-no-include"

  fun apply(h: TestHelper) =>
    // Private type with include_private=false: no link generated
    let t = doc.DocNominal("_Foo", "pkg-_Foo",
      recover val Array[doc.DocType] end,
      None, None, true, false)
    h.assert_eq[String](
      doc.TypeRenderer.render(t, true, false, false), "_Foo")

    // Same type with include_private=true: link generated
    h.assert_eq[String](
      doc.TypeRenderer.render(t, true, false, true),
      "[_Foo](pkg-_Foo.md)")

class \nodoc\ _TestRenderNominalAnonymous is UnitTest
  fun name(): String => "TypeRenderer/nominal-anonymous"

  fun apply(h: TestHelper) =>
    // Anonymous (compiler-generated) types never get links
    let t = doc.DocNominal("$1", "pkg-$1",
      recover val Array[doc.DocType] end,
      None, None, false, true)
    h.assert_eq[String](
      doc.TypeRenderer.render(t, true, false, true), "$1")

class \nodoc\ _TestRenderUnion is UnitTest
  fun name(): String => "TypeRenderer/union"

  fun apply(h: TestHelper) =>
    let string_t = doc.DocNominal("String", "builtin-String",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let none_t = doc.DocNominal("None", "builtin-None",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let u = doc.DocUnion(
      recover val [as doc.DocType: string_t; none_t] end)

    h.assert_eq[String](
      doc.TypeRenderer.render(u, false, false, false),
      "(String | None)")

    h.assert_eq[String](
      doc.TypeRenderer.render(u, true, false, false),
      "([String](builtin-String.md) | [None](builtin-None.md))")

class \nodoc\ _TestRenderIntersection is UnitTest
  fun name(): String => "TypeRenderer/intersection"

  fun apply(h: TestHelper) =>
    let a = doc.DocNominal("Stringable", "builtin-Stringable",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let b = doc.DocNominal("Hashable", "collections-Hashable",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let i = doc.DocIntersection(
      recover val [as doc.DocType: a; b] end)

    h.assert_eq[String](
      doc.TypeRenderer.render(i, false, false, false),
      "(Stringable & Hashable)")

class \nodoc\ _TestRenderTuple is UnitTest
  fun name(): String => "TypeRenderer/tuple"

  fun apply(h: TestHelper) =>
    let s = doc.DocNominal("String", "builtin-String",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let u = doc.DocNominal("U32", "builtin-U32",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let t = doc.DocTuple(
      recover val [as doc.DocType: s; u] end)

    h.assert_eq[String](
      doc.TypeRenderer.render(t, false, false, false),
      "(String , U32)")

class \nodoc\ _TestRenderArrow is UnitTest
  fun name(): String => "TypeRenderer/arrow"

  fun apply(h: TestHelper) =>
    let this_t = doc.DocThis
    let param_ref = doc.DocTypeParamRef("T", None)
    let a = doc.DocArrow(this_t, param_ref)

    h.assert_eq[String](
      doc.TypeRenderer.render(a, false, false, false),
      "this->T")

class \nodoc\ _TestRenderThis is UnitTest
  fun name(): String => "TypeRenderer/this"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.TypeRenderer.render(doc.DocThis, false, false, false),
      "this")

class \nodoc\ _TestRenderCapability is UnitTest
  fun name(): String => "TypeRenderer/capability"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.TypeRenderer.render(doc.DocCapability("iso"), false, false, false),
      "iso")
    h.assert_eq[String](
      doc.TypeRenderer.render(doc.DocCapability("#read"), false, false, false),
      "#read")

class \nodoc\ _TestRenderTypeParamRef is UnitTest
  fun name(): String => "TypeRenderer/type-param-ref"

  fun apply(h: TestHelper) =>
    let t = doc.DocTypeParamRef("A", None)
    h.assert_eq[String](
      doc.TypeRenderer.render(t, false, false, false), "A")

class \nodoc\ _TestRenderTypeParamRefEphemeral is UnitTest
  fun name(): String => "TypeRenderer/type-param-ref-ephemeral"

  fun apply(h: TestHelper) =>
    let t = doc.DocTypeParamRef("T", "^")
    h.assert_eq[String](
      doc.TypeRenderer.render(t, false, false, false), "T^")

class \nodoc\ _TestRenderTypeParams is UnitTest
  fun name(): String => "TypeRenderer/type-params"

  fun apply(h: TestHelper) =>
    let constraint = doc.DocNominal("Any", "builtin-Any",
      recover val Array[doc.DocType] end,
      "val", None, false, false)
    let tp = doc.DocTypeParam("A", constraint, None)
    let tps: Array[doc.DocTypeParam] val =
      recover val [as doc.DocTypeParam: tp] end

    // With links: escaped brackets
    h.assert_eq[String](
      doc.TypeRenderer.render_type_params(tps, true, false, false),
      "\\[A: [Any](builtin-Any.md) val\\]")

    // Without links: plain brackets
    h.assert_eq[String](
      doc.TypeRenderer.render_type_params(tps, false, false, false),
      "[A: Any val]")

class \nodoc\ _TestRenderTypeParamsOptional is UnitTest
  fun name(): String => "TypeRenderer/type-params-optional"

  fun apply(h: TestHelper) =>
    let constraint = doc.DocNominal("Any", "builtin-Any",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let default_type = doc.DocNominal("None", "builtin-None",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let tp = doc.DocTypeParam("A", constraint, default_type)
    let tps: Array[doc.DocTypeParam] val =
      recover val [as doc.DocTypeParam: tp] end

    h.assert_eq[String](
      doc.TypeRenderer.render_type_params(tps, false, false, false),
      "[optional A: Any]")

class \nodoc\ _TestRenderTypeParamsNoConstraint is UnitTest
  fun name(): String => "TypeRenderer/type-params-no-constraint"

  fun apply(h: TestHelper) =>
    let tp = doc.DocTypeParam("A", None, None)
    let tps: Array[doc.DocTypeParam] val =
      recover val [as doc.DocTypeParam: tp] end

    h.assert_eq[String](
      doc.TypeRenderer.render_type_params(tps, false, false, false),
      "[A: no constraint]")

class \nodoc\ _TestRenderTypeParamsEmpty is UnitTest
  fun name(): String => "TypeRenderer/type-params-empty"

  fun apply(h: TestHelper) =>
    let tps: Array[doc.DocTypeParam] val =
      recover val Array[doc.DocTypeParam] end
    h.assert_eq[String](
      doc.TypeRenderer.render_type_params(tps, true, false, false), "")

class \nodoc\ _TestRenderNominalCapEphemeral is UnitTest
  fun name(): String => "TypeRenderer/nominal-cap-ephemeral"

  fun apply(h: TestHelper) =>
    let t = doc.DocNominal("Foo", "pkg-Foo",
      recover val Array[doc.DocType] end,
      "iso", "^", false, false)
    h.assert_eq[String](
      doc.TypeRenderer.render(t, false, false, false), "Foo iso^")

class \nodoc\ _TestRenderBreakLines is UnitTest
  fun name(): String => "TypeRenderer/break-lines"

  fun apply(h: TestHelper) =>
    // 4 types in a union with break_lines=true:
    // line break after every 3rd separator (i.e. after the 3rd "|")
    let a = doc.DocNominal("A", "p-A",
      recover val Array[doc.DocType] end, None, None, false, false)
    let b = doc.DocNominal("B", "p-B",
      recover val Array[doc.DocType] end, None, None, false, false)
    let c = doc.DocNominal("C", "p-C",
      recover val Array[doc.DocType] end, None, None, false, false)
    let d = doc.DocNominal("D", "p-D",
      recover val Array[doc.DocType] end, None, None, false, false)
    let u = doc.DocUnion(
      recover val [as doc.DocType: a; b; c; d] end)

    // break_lines=true inserts "\n    " after the 3rd separator
    h.assert_eq[String](
      doc.TypeRenderer.render(u, false, true, false),
      "(A | B | C | \n    D)")

    // break_lines=false: no line break
    h.assert_eq[String](
      doc.TypeRenderer.render(u, false, false, false),
      "(A | B | C | D)")

class \nodoc\ _TestRenderProvides is UnitTest
  fun name(): String => "TypeRenderer/provides"

  fun apply(h: TestHelper) =>
    let a = doc.DocNominal("Stringable", "builtin-Stringable",
      recover val Array[doc.DocType] end, None, None, false, false)
    let b = doc.DocNominal("Hashable", "collections-Hashable",
      recover val Array[doc.DocType] end, None, None, false, false)
    let provides: Array[doc.DocType] val =
      recover val [as doc.DocType: a; b] end

    // With links
    h.assert_eq[String](
      doc.TypeRenderer.render_provides(provides,
        "* ", "\n* ", "\n", true, false),
      "* [Stringable](builtin-Stringable.md)\n* [Hashable](collections-Hashable.md)\n")

    // Without links
    h.assert_eq[String](
      doc.TypeRenderer.render_provides(provides,
        "is\n  ", " &\n  ", "\n", false, false),
      "is\n  Stringable &\n  Hashable\n")
