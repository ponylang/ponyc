use "pony_test"
use "../workspace"

primitive _InlayHintSourceTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_InlayHintSourceIsCapTrueTest)
    test(_InlayHintSourceIsCapFalseTest)
    test(_InlayHintSourceIsCapLongWordTest)
    test(_InlayHintSourceIsCapErrorTest)
    test(_InlayHintSourceScanPastBracketsSimpleTest)
    test(_InlayHintSourceScanPastBracketsNestedTest)
    test(_InlayHintSourceScanPastBracketsErrorTest)
    test(_InlayHintSourceScanPastBracketsMultilineTest)
    test(_InlayHintSourceHasAnnotationColonTest)
    test(_InlayHintSourceHasAnnotationWhitespaceTest)
    test(_InlayHintSourceHasAnnotationTabTest)
    test(_InlayHintSourceHasAnnotationEndOfStringTest)
    test(_InlayHintSourceHasAnnotationEqualsTest)
    test(_InlayHintSourceHasAnnotationNewlineTest)
    test(_InlayHintSourceHasAnnotationOtherTest)
    test(_InlayHintSourceHasExplicitCapAfterCapTest)
    test(_InlayHintSourceHasExplicitCapAfterSpaceCapTest)
    test(_InlayHintSourceHasExplicitCapAfterNonCapTest)
    test(_InlayHintSourceHasExplicitCapAfterLongWordTest)
    test(_InlayHintSourceHasExplicitCapAfterNonAlphaTest)
    test(_InlayHintSourceScanToCloseParenSimpleTest)
    test(_InlayHintSourceScanToCloseParenNestedTest)
    test(_InlayHintSourceScanToCloseParenBracketsTest)
    test(_InlayHintSourceScanToCloseParenMultilineTest)
    test(_InlayHintSourceScanToCloseParenErrorTest)
    test(_InlayHintSourceScanToCloseParenStringLiteralTest)
    test(_InlayHintSourceScanToCloseParenCharLiteralTest)
    test(_InlayHintSourceScanToCloseParenEscapedDelimiterTest)
    test(_InlayHintSourceScanToCloseParenParensInBracketsTest)
    test(_InlayHintSourceExplicitReturnTypeColonTest)
    test(_InlayHintSourceExplicitReturnTypeArrowTest)
    test(_InlayHintSourceExplicitReturnTypeSkipTest)
    test(_InlayHintSourceExplicitReturnTypeTabNewlineSkipTest)
    test(_InlayHintSourceExplicitReturnTypeEqualNotArrowTest)
    test(_InlayHintSourceExplicitReturnTypeErrorTest)
    test(_InlayHintSourceHasExplicitReceiverCapCapTest)
    test(_InlayHintSourceHasExplicitReceiverCapTabTest)
    test(_InlayHintSourceHasExplicitReceiverCapFunTest)
    test(_InlayHintSourceHasExplicitReceiverCapLongWordTest)
    test(_InlayHintSourceHasExplicitReceiverCapUnexpectedTest)
    test(_InlayHintSourceHasExplicitReceiverCapStartOfFileTest)
    test(_InlayHintSourceHasExplicitReceiverCapAnnotationNocapTest)
    test(_InlayHintSourceHasExplicitReceiverCapAnnotationWithCapTest)

class \nodoc\ iso _InlayHintSourceIsCapTrueTest is UnitTest
  fun name(): String => "inlay_hint_source/is_cap/true"

  fun apply(h: TestHelper) =>
    try
      h.assert_true(InlayHintSource.is_cap("iso", 0)?)
      h.assert_true(InlayHintSource.is_cap("trn", 0)?)
      h.assert_true(InlayHintSource.is_cap("ref", 0)?)
      h.assert_true(InlayHintSource.is_cap("val", 0)?)
      h.assert_true(InlayHintSource.is_cap("box", 0)?)
      h.assert_true(InlayHintSource.is_cap("tag", 0)?)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceIsCapFalseTest is UnitTest
  fun name(): String => "inlay_hint_source/is_cap/false"

  fun apply(h: TestHelper) =>
    try
      h.assert_false(InlayHintSource.is_cap("fun", 0)?)
      h.assert_false(InlayHintSource.is_cap("let", 0)?)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceIsCapLongWordTest is UnitTest
  fun name(): String => "inlay_hint_source/is_cap/long_word"

  fun apply(h: TestHelper) =>
    try
      // "iso" prefix followed by more letters — not a cap keyword
      h.assert_false(InlayHintSource.is_cap("isolation", 0)?)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceIsCapErrorTest is UnitTest
  fun name(): String => "inlay_hint_source/is_cap/error"

  fun apply(h: TestHelper) =>
    // only 2 bytes available starting at j, so j+2 is out of bounds
    h.assert_true(
      try InlayHintSource.is_cap("ab", 0)? ; false else true end)

class \nodoc\ iso _InlayHintSourceScanPastBracketsSimpleTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_past_brackets/simple"

  fun apply(h: TestHelper) =>
    try
      (let j, let jl, let jc) =
        InlayHintSource.scan_past_brackets("[X]", 0, 1, 1)?
      h.assert_eq[USize](3, j)    // byte index after ']'
      h.assert_eq[USize](1, jl)   // still on line 1
      h.assert_eq[USize](4, jc)   // col immediately after ']'
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceScanPastBracketsNestedTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_past_brackets/nested"

  fun apply(h: TestHelper) =>
    try
      (let j, let jl, let jc) =
        InlayHintSource.scan_past_brackets("[X[Y]]", 0, 1, 1)?
      h.assert_eq[USize](6, j)
      h.assert_eq[USize](1, jl)
      h.assert_eq[USize](7, jc)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceScanPastBracketsErrorTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_past_brackets/error"

  fun apply(h: TestHelper) =>
    // unclosed bracket
    h.assert_true(
      try
        InlayHintSource.scan_past_brackets("[X", 0, 1, 1)?
        false
      else
        true
      end)

class \nodoc\ iso _InlayHintSourceHasAnnotationColonTest is UnitTest
  fun name(): String => "inlay_hint_source/has_annotation/colon"

  fun apply(h: TestHelper) =>
    // ':' immediately follows the name
    h.assert_true(InlayHintSource.has_annotation("x: String", 1))

class \nodoc\ iso _InlayHintSourceHasAnnotationWhitespaceTest is UnitTest
  fun name(): String => "inlay_hint_source/has_annotation/whitespace"

  fun apply(h: TestHelper) =>
    // whitespace between name and ':'
    h.assert_true(InlayHintSource.has_annotation("x  : String", 1))

class \nodoc\ iso _InlayHintSourceHasAnnotationEqualsTest is UnitTest
  fun name(): String => "inlay_hint_source/has_annotation/equals"

  fun apply(h: TestHelper) =>
    // '=' stops scan before ':'
    h.assert_false(InlayHintSource.has_annotation("x = val", 1))

class \nodoc\ iso _InlayHintSourceHasAnnotationNewlineTest is UnitTest
  fun name(): String => "inlay_hint_source/has_annotation/newline"

  fun apply(h: TestHelper) =>
    // '\n' stops scan before ':'
    h.assert_false(InlayHintSource.has_annotation("x\n: String", 1))

class \nodoc\ iso _InlayHintSourceHasAnnotationOtherTest is UnitTest
  fun name(): String => "inlay_hint_source/has_annotation/other_char"

  fun apply(h: TestHelper) =>
    // unexpected character (letter) stops scan
    h.assert_false(InlayHintSource.has_annotation("x val", 1))

class \nodoc\ iso _InlayHintSourceHasExplicitCapAfterCapTest is UnitTest
  fun name(): String => "inlay_hint_source/has_explicit_cap_after/cap"

  fun apply(h: TestHelper) =>
    h.assert_true(InlayHintSource.has_explicit_cap_after("ref", 0))
    h.assert_true(InlayHintSource.has_explicit_cap_after("iso", 0))
    h.assert_true(InlayHintSource.has_explicit_cap_after("val", 0))

class \nodoc\ iso _InlayHintSourceHasExplicitCapAfterSpaceCapTest is UnitTest
  fun name(): String =>
    "inlay_hint_source/has_explicit_cap_after/space_then_cap"

  fun apply(h: TestHelper) =>
    h.assert_true(InlayHintSource.has_explicit_cap_after(" box", 0))
    h.assert_true(InlayHintSource.has_explicit_cap_after("  tag", 0))

class \nodoc\ iso _InlayHintSourceHasExplicitCapAfterNonCapTest is UnitTest
  fun name(): String => "inlay_hint_source/has_explicit_cap_after/non_cap"

  fun apply(h: TestHelper) =>
    // 3-letter word that is not a cap
    h.assert_false(InlayHintSource.has_explicit_cap_after("fun", 0))

class \nodoc\ iso _InlayHintSourceHasExplicitCapAfterLongWordTest is UnitTest
  fun name(): String => "inlay_hint_source/has_explicit_cap_after/long_word"

  fun apply(h: TestHelper) =>
    // word longer than 3 letters is not a cap
    h.assert_false(InlayHintSource.has_explicit_cap_after("isolation", 0))

class \nodoc\ iso _InlayHintSourceHasExplicitCapAfterNonAlphaTest is UnitTest
  fun name(): String => "inlay_hint_source/has_explicit_cap_after/non_alpha"

  fun apply(h: TestHelper) =>
    // non-letter character: not a cap
    h.assert_false(InlayHintSource.has_explicit_cap_after("42", 0))

class \nodoc\ iso _InlayHintSourceScanToCloseParenSimpleTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_to_close_paren/simple"

  fun apply(h: TestHelper) =>
    // "f()": ')' is at 1-based col 3; col after = 4; j_after = 3
    try
      (let j_after, let line, let col) =
        InlayHintSource.scan_to_close_paren("f()", 0, 1, 1)?
      h.assert_eq[USize](3, j_after)
      h.assert_eq[USize](1, line)
      h.assert_eq[USize](4, col)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceScanToCloseParenNestedTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_to_close_paren/nested"

  fun apply(h: TestHelper) =>
    // "f(g())": inner ')' at col 5, outer ')' at col 6; col after = 7
    try
      (let j_after, let line, let col) =
        InlayHintSource.scan_to_close_paren("f(g())", 0, 1, 1)?
      h.assert_eq[USize](6, j_after)
      h.assert_eq[USize](1, line)
      h.assert_eq[USize](7, col)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceScanToCloseParenBracketsTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_to_close_paren/brackets"

  fun apply(h: TestHelper) =>
    // "f[T]()": '[' and ']' are skipped; ')' at col 6; col after = 7
    try
      (let j_after, let line, let col) =
        InlayHintSource.scan_to_close_paren("f[T]()", 0, 1, 1)?
      h.assert_eq[USize](6, j_after)
      h.assert_eq[USize](1, line)
      h.assert_eq[USize](7, col)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceScanToCloseParenErrorTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_to_close_paren/error"

  fun apply(h: TestHelper) =>
    // unclosed paren
    h.assert_true(
      try
        InlayHintSource.scan_to_close_paren("f(", 0, 1, 1)?
        false
      else
        true
      end)

class \nodoc\ iso _InlayHintSourceScanToCloseParenStringLiteralTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_to_close_paren/string_literal"

  fun apply(h: TestHelper) =>
    // ')' inside a string literal must be skipped
    // outer ')' at col 6; after = 7
    try
      (let j_after, let line, let col) =
        InlayHintSource.scan_to_close_paren("f(\")\")", 0, 1, 1)?
      h.assert_eq[USize](6, j_after)
      h.assert_eq[USize](1, line)
      h.assert_eq[USize](7, col)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceScanToCloseParenCharLiteralTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_to_close_paren/char_literal"

  fun apply(h: TestHelper) =>
    // ')' inside a char literal must be skipped; outer ')' at col 6; after = 7
    try
      (let j_after, let line, let col) =
        InlayHintSource.scan_to_close_paren("f(')')", 0, 1, 1)?
      h.assert_eq[USize](6, j_after)
      h.assert_eq[USize](1, line)
      h.assert_eq[USize](7, col)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceScanToCloseParenEscapedDelimiterTest
  is UnitTest
  fun name(): String =>
    "inlay_hint_source/scan_to_close_paren/escaped_delimiter"

  fun apply(h: TestHelper) =>
    // "f(\"a\")\")": \" inside string is an escaped quote; ')' at pos 6
    // is inside the string and must be skipped; outer ')' at pos 8; after = 9
    try
      (let j_after, let line, let col) =
        InlayHintSource.scan_to_close_paren("f(\"a\\\")\")", 0, 1, 1)?
      h.assert_eq[USize](9, j_after)
      h.assert_eq[USize](1, line)
      h.assert_eq[USize](10, col)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceScanToCloseParenParensInBracketsTest
  is UnitTest
  fun name(): String =>
    "inlay_hint_source/scan_to_close_paren/parens_in_brackets"

  fun apply(h: TestHelper) =>
    // "f[()]()": ')' inside brackets is skipped; outer ')' at col 7; after = 8
    try
      (let j_after, let line, let col) =
        InlayHintSource.scan_to_close_paren("f[()]()", 0, 1, 1)?
      h.assert_eq[USize](7, j_after)
      h.assert_eq[USize](1, line)
      h.assert_eq[USize](8, col)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceExplicitReturnTypeColonTest is UnitTest
  fun name(): String => "inlay_hint_source/explicit_return_type/colon"

  fun apply(h: TestHelper) =>
    try
      h.assert_true(InlayHintSource.has_explicit_return_type(": String", 0)?)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceExplicitReturnTypeArrowTest is UnitTest
  fun name(): String => "inlay_hint_source/explicit_return_type/arrow"

  fun apply(h: TestHelper) =>
    try
      h.assert_false(InlayHintSource.has_explicit_return_type("=>", 0)?)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceExplicitReturnTypeSkipTest is UnitTest
  fun name(): String =>
    "inlay_hint_source/explicit_return_type/skip_whitespace_question"

  fun apply(h: TestHelper) =>
    // spaces and '?' are skipped before ':'
    try
      h.assert_true(InlayHintSource.has_explicit_return_type(" ?: String", 0)?)
      // spaces and '?' are skipped before '=>'
      h.assert_false(InlayHintSource.has_explicit_return_type(" ? =>", 0)?)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceExplicitReturnTypeErrorTest is UnitTest
  fun name(): String => "inlay_hint_source/explicit_return_type/error"

  fun apply(h: TestHelper) =>
    // unexpected character
    h.assert_true(
      try
        InlayHintSource.has_explicit_return_type("x", 0)?
        false
      else
        true
      end)

class \nodoc\ iso _InlayHintSourceHasExplicitReceiverCapCapTest is UnitTest
  fun name(): String => "inlay_hint_source/has_explicit_receiver_cap/cap"

  fun apply(h: TestHelper) =>
    // "fun ref foo": cap 'ref' is between 'fun' and name; name_start = 8
    h.assert_true(InlayHintSource.has_explicit_receiver_cap("fun ref foo", 8))
    // "fun val foo": name_start = 8
    h.assert_true(InlayHintSource.has_explicit_receiver_cap("fun val foo", 8))

class \nodoc\ iso _InlayHintSourceHasExplicitReceiverCapFunTest is UnitTest
  fun name(): String => "inlay_hint_source/has_explicit_receiver_cap/fun"

  fun apply(h: TestHelper) =>
    // "fun foo": no cap between 'fun' and name; name_start = 4
    h.assert_false(InlayHintSource.has_explicit_receiver_cap("fun foo", 4))

class \nodoc\ iso _InlayHintSourceHasExplicitReceiverCapLongWordTest is UnitTest
  fun name(): String => "inlay_hint_source/has_explicit_receiver_cap/long_word"

  fun apply(h: TestHelper) =>
    // word before name is longer than 3 letters, so not a cap
    h.assert_false(
      InlayHintSource.has_explicit_receiver_cap("fun longer foo", 11))

class \nodoc\ iso _InlayHintSourceHasExplicitReceiverCapUnexpectedTest
  is UnitTest
  fun name(): String =>
    "inlay_hint_source/has_explicit_receiver_cap/unexpected_char"

  fun apply(h: TestHelper) =>
    // non-letter char before name: skip hint (return true)
    h.assert_true(InlayHintSource.has_explicit_receiver_cap("100", 3))

class \nodoc\ iso _InlayHintSourceScanPastBracketsMultilineTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_past_brackets/multiline"

  fun apply(h: TestHelper) =>
    // "[\nX]": newline increments line counter; ']' is on line 2, col 3
    try
      (let j, let jl, let jc) =
        InlayHintSource.scan_past_brackets("[\nX]", 0, 1, 1)?
      h.assert_eq[USize](4, j)
      h.assert_eq[USize](2, jl)
      h.assert_eq[USize](3, jc)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceHasAnnotationTabTest is UnitTest
  fun name(): String => "inlay_hint_source/has_annotation/tab"

  fun apply(h: TestHelper) =>
    // tab is treated as whitespace before ':'
    h.assert_true(InlayHintSource.has_annotation("x\t: String", 1))

class \nodoc\ iso _InlayHintSourceHasAnnotationEndOfStringTest is UnitTest
  fun name(): String => "inlay_hint_source/has_annotation/end_of_string"

  fun apply(h: TestHelper) =>
    // loop exhausts without finding ':' — returns false
    h.assert_false(InlayHintSource.has_annotation("x", 1))

class \nodoc\ iso _InlayHintSourceScanToCloseParenMultilineTest is UnitTest
  fun name(): String => "inlay_hint_source/scan_to_close_paren/multiline"

  fun apply(h: TestHelper) =>
    // "f(\n)": ')' is on line 2, col 1; col after = 2; j_after = 4
    try
      (let j_after, let line, let col) =
        InlayHintSource.scan_to_close_paren("f(\n)", 0, 1, 1)?
      h.assert_eq[USize](4, j_after)
      h.assert_eq[USize](2, line)
      h.assert_eq[USize](2, col)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceExplicitReturnTypeTabNewlineSkipTest
  is UnitTest
  fun name(): String =>
    "inlay_hint_source/explicit_return_type/skip_tab_newline"

  fun apply(h: TestHelper) =>
    // tab and newline are skipped like space
    try
      h.assert_true(InlayHintSource.has_explicit_return_type("\t\n: T", 0)?)
    else
      h.fail("unexpected error")
    end

class \nodoc\ iso _InlayHintSourceExplicitReturnTypeEqualNotArrowTest
  is UnitTest
  fun name(): String =>
    "inlay_hint_source/explicit_return_type/equal_not_arrow"

  fun apply(h: TestHelper) =>
    // '=' not followed by '>' errors
    h.assert_true(
      try
        InlayHintSource.has_explicit_return_type("= x", 0)?
        false
      else
        true
      end)

class \nodoc\ iso _InlayHintSourceHasExplicitReceiverCapTabTest is UnitTest
  fun name(): String => "inlay_hint_source/has_explicit_receiver_cap/tab"

  fun apply(h: TestHelper) =>
    // tab between 'fun' and name is treated as whitespace
    h.assert_false(InlayHintSource.has_explicit_receiver_cap("fun\tfoo", 4))

class \nodoc\ iso _InlayHintSourceHasExplicitReceiverCapStartOfFileTest
  is UnitTest
  fun name(): String =>
    "inlay_hint_source/has_explicit_receiver_cap/start_of_file"

  fun apply(h: TestHelper) =>
    // name_start = 0: loop body never executes — returns true (skip hint)
    h.assert_true(InlayHintSource.has_explicit_receiver_cap("foo", 0))

class \nodoc\ iso _InlayHintSourceHasExplicitReceiverCapAnnotationNocapTest
  is UnitTest
  fun name(): String =>
    "inlay_hint_source/has_explicit_receiver_cap/annotation_no_cap"

  fun apply(h: TestHelper) =>
    // "fun \nodoc\ foo": annotation between fun and name, no explicit cap
    // backward scan must skip \nodoc\ and recognise 'fun' — return false
    h.assert_false(
      InlayHintSource.has_explicit_receiver_cap("fun \\nodoc\\ foo", 12))

class \nodoc\ iso _InlayHintSourceHasExplicitReceiverCapAnnotationWithCapTest
  is UnitTest
  fun name(): String =>
    "inlay_hint_source/has_explicit_receiver_cap/annotation_with_cap"

  fun apply(h: TestHelper) =>
    // "fun \nodoc\ box foo": annotation then explicit cap — return true
    h.assert_true(
      InlayHintSource.has_explicit_receiver_cap("fun \\nodoc\\ box foo", 16))
