use "pony_test"
use "../workspace"

primitive _DocumentHighlightSourceTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_HighlightSourceHasInitializerTest)
    test(_HighlightSourceNoInitializerTest)
    test(_HighlightSourceEqInCommentTest)
    test(_HighlightSourceFletInitializerTest)
    test(_HighlightSourceFletNoInitializerTest)
    test(_HighlightSourceEmbedInitializerTest)
    test(_HighlightSourceEmbedNoInitializerTest)
    test(_HighlightSourceMultilineInitializerTest)
    test(_HighlightSourceMultilineNoInitializerTest)

class \nodoc\ iso _HighlightSourceHasInitializerTest is UnitTest
  fun name(): String => "highlight_source/field_has_initializer/with_init"

  fun apply(h: TestHelper) =>
    let src = "var with_init: U32 = 0"
    h.assert_true(DocumentHighlightSource.field_has_initializer(src, 0))

class \nodoc\ iso _HighlightSourceNoInitializerTest is UnitTest
  fun name(): String => "highlight_source/field_has_initializer/without_init"

  fun apply(h: TestHelper) =>
    let src = "var without_init: U32"
    h.assert_false(DocumentHighlightSource.field_has_initializer(src, 0))

class \nodoc\ iso _HighlightSourceEqInCommentTest is UnitTest
  fun name(): String => "highlight_source/field_has_initializer/eq_in_comment"

  fun apply(h: TestHelper) =>
    let src = "var eq_in_comment: U32 // default=0"
    h.assert_false(DocumentHighlightSource.field_has_initializer(src, 0))

class \nodoc\ iso _HighlightSourceFletInitializerTest is UnitTest
  fun name(): String =>
    "highlight_source/field_has_initializer/flet_with_init"

  fun apply(h: TestHelper) =>
    let src = "let flet_with_init: Bool = false"
    h.assert_true(DocumentHighlightSource.field_has_initializer(src, 0))

class \nodoc\ iso _HighlightSourceFletNoInitializerTest is UnitTest
  fun name(): String =>
    "highlight_source/field_has_initializer/flet_without_init"

  fun apply(h: TestHelper) =>
    let src = "let flet_without_init: Bool"
    h.assert_false(DocumentHighlightSource.field_has_initializer(src, 0))

class \nodoc\ iso _HighlightSourceEmbedInitializerTest is UnitTest
  fun name(): String =>
    "highlight_source/field_has_initializer/embed_with_init"

  fun apply(h: TestHelper) =>
    let src = "embed e: _Inner = _Inner.create()"
    h.assert_true(DocumentHighlightSource.field_has_initializer(src, 0))

class \nodoc\ iso _HighlightSourceEmbedNoInitializerTest is UnitTest
  fun name(): String =>
    "highlight_source/field_has_initializer/embed_without_init"

  fun apply(h: TestHelper) =>
    let src = "embed e: _Inner"
    h.assert_false(DocumentHighlightSource.field_has_initializer(src, 0))

class \nodoc\ iso _HighlightSourceMultilineInitializerTest is UnitTest
  fun name(): String =>
    "highlight_source/field_has_initializer/multiline_with_init"

  fun apply(h: TestHelper) =>
    // `=` is on a continuation line (more indented than field keyword)
    let src = "  var x: U32\n    = 0\n  var y: U32"
    h.assert_true(DocumentHighlightSource.field_has_initializer(src, 2))

class \nodoc\ iso _HighlightSourceMultilineNoInitializerTest is UnitTest
  fun name(): String =>
    "highlight_source/field_has_initializer/multiline_without_init"

  fun apply(h: TestHelper) =>
    // next line is at same indentation — a new declaration with its own `=`,
    // not a continuation of x. Must return false for x even though `=` appears
    // on the next line.
    let src = "  var x: U32\n  var y: U32 = 0"
    h.assert_false(DocumentHighlightSource.field_has_initializer(src, 2))
