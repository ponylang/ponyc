use "pony_test"
use "random"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically.
    test(_TestIniParse)
    test(_TestIniParseBlankLinesIgnored)
    test(_TestIniParseBracketSectionPreservesInnerWhitespace)
    test(_TestIniParseCaseSensitive)
    test(_TestIniParseCommentCharsInKeyLiteral)
    test(_TestIniParseCommentMarkerAtValueIndexZeroLiteral)
    test(_TestIniParseDelimiterSelection)
    test(_TestIniParseDuplicateKeyLastWins)
    test(_TestIniParseEmptyBrackets)
    test(_TestIniParseEmptyInput)
    test(_TestIniParseEmptyKeyAndValueAllowed)
    test(_TestIniParseEqualsInValueKept)
    test(_TestIniParseFirstClosingBracketWins)
    test(_TestIniParseFullLineCommentsSkipped)
    test(_TestIniParseGlobalSectionForPreHeaderKeys)
    test(_TestIniParseInlineCommentInsideQuotedLooking)
    test(_TestIniParseInlineCommentRequiresSpaceOrTab)
    test(_TestIniParseInternalKeyAndValueWhitespacePreserved)
    test(_TestIniParseKeyAndValueIndividuallyStripped)
    test(_TestIniParseLooksLikeSectionButIsnt)
    test(_TestIniParseNestedSectionNotInterpreted)
    test(_TestIniParseQuoteCharsLiteral)
    test(_TestIniParseRaisesOnIncompleteSection)
    test(_TestIniParseRaisesOnNoDelimiter)
    test(_TestIniParseRepeatedSectionMerges)
    test(_TestIniParseSectionEmptyMap)
    test(_TestIniParseSectionHeaderBeatsKv)
    test(_TestIniParseSectionNameWithDelimitersAndCommentChars)
    test(_TestIniParseSemicolonShadowsLaterComment)
    test(_TestIniParseSingleCharLines)
    test(_TestIniParseTextAfterClosingBracketDiscarded)
    test(_TestIniParseTrailingWhitespaceBeforeCommentStripped)
    test(_TestIniParseUtf8Passthrough)
    test(_TestIniPropertyCommentLineRecognition)
    test(_TestIniPropertyLineLevelTrimInvariance)
    test(_TestIniStreamingAddSectionHaltsAfterErrorReturnsFalse)
    test(_TestIniStreamingAddSectionHaltsCleanReturnsTrue)
    test(_TestIniStreamingApplyHaltsAfterErrorReturnsFalse)
    test(_TestIniStreamingApplyHaltsCleanReturnsTrue)
    test(_TestIniStreamingBareOpenBracketReportsIncompleteSection)
    test(_TestIniStreamingCallOrderPreservesInputOrder)
    test(_TestIniStreamingCaseSensitiveVerbatim)
    test(_TestIniStreamingDefaultErrorsContinuesParsing)
    test(_TestIniStreamingDuplicateKeyFiresAll)
    test(_TestIniStreamingEmptyIteratorReturnsTrue)
    test(_TestIniStreamingErrorsContinueButFinalFalse)
    test(_TestIniStreamingErrorsHaltStopsImmediately)
    test(_TestIniStreamingIncompleteSectionDoesNotChangeCurrentSection)
    test(_TestIniStreamingLineNumbersOneBased)
    test(_TestIniStreamingNoAddSectionForImplicitGlobal)
    test(_TestIniStreamingNoDelimiterReports)

// A recording IniNotify used by the streaming tests. Each test creates its
// own instance; nothing is shared between tests. The three `*_returns`
// fields let a test configure the notifier to halt by returning `false`
// from a specific callback.
class \nodoc\ ref _RecordingNotify is IniNotify
  let kvs: Array[(String, String, String)] = kvs.create()
  let sections: Array[String] = sections.create()
  let errs: Array[(USize, IniError)] = errs.create()
  var apply_returns: Bool = true
  var add_section_returns: Bool = true
  var errors_returns: Bool = true

  new ref create() => None

  fun ref apply(section: String, key: String, value: String): Bool =>
    kvs.push((section, key, value))
    apply_returns

  fun ref add_section(section: String): Bool =>
    sections.push(section)
    add_section_returns

  fun ref errors(line: USize, err: IniError): Bool =>
    errs.push((line, err))
    errors_returns

class \nodoc\ iso _TestIniParse is UnitTest
  fun name(): String => "ini/IniParse"

  fun apply(h: TestHelper) ? =>
    let source =
      """
      key 1 = value 1 ; Not in a section
      [Section 1] # First section

      key 2 =     value 2

      [Section 2]

      [Section 3]
      key 2 = value 3
             key 1   =   value 1;Not a comment
      """

    let array: Array[String] = source.split("\r\n")
    let map = IniParse(array.values())?

    h.assert_eq[Bool](map.contains("Section 2"), true)

    h.assert_eq[String](map("")?("key 1")?, "value 1")
    h.assert_eq[String](map("Section 1")?("key 2")?, "value 2")
    h.assert_eq[String](map("Section 3")?("key 2")?, "value 3")
    h.assert_eq[String](map("Section 3")?("key 1")?, "value 1;Not a comment")

class \nodoc\ iso _TestIniParseBlankLinesIgnored is UnitTest
  fun name(): String => "ini/ParseBlankLinesIgnored"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      ""
      "   "
      "\t"
      " \t \t "
      "k=v" ]
    let map = IniParse(input.values())?
    h.assert_eq[USize](map.size(), 1)
    h.assert_eq[String](map("")?("k")?, "v")

class \nodoc\ iso _TestIniParseBracketSectionPreservesInnerWhitespace is
  UnitTest
  // Pins the documented quirk that text between `[` and `]` is not trimmed.
  // Tracked as a potential bug in ponylang/ponyc#5326; this test pins the
  // current behavior. If the parser is changed to trim section names, update
  // this test to match the new behavior.
  fun name(): String => "ini/ParseBracketSectionPreservesInnerWhitespace"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[ name ]"
      "k=v" ]
    let map = IniParse(input.values())?
    h.assert_true(map.contains(" name "))
    h.assert_false(map.contains("name"))
    h.assert_eq[String](map(" name ")?("k")?, "v")

class \nodoc\ iso _TestIniParseCaseSensitive is UnitTest
  fun name(): String => "ini/ParseCaseSensitive"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[Section]"
      "Key=A"
      "key=B"
      "[section]"
      "k=other" ]
    let map = IniParse(input.values())?
    h.assert_true(map.contains("Section"))
    h.assert_true(map.contains("section"))
    h.assert_eq[String](map("Section")?("Key")?, "A")
    h.assert_eq[String](map("Section")?("key")?, "B")
    h.assert_eq[String](map("section")?("k")?, "other")

class \nodoc\ iso _TestIniParseCommentCharsInKeyLiteral is UnitTest
  fun name(): String => "ini/ParseCommentCharsInKeyLiteral"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "a;b=c"
      "a#b=d"
      "a;#=e" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("a;b")?, "c")
    h.assert_eq[String](map("")?("a#b")?, "d")
    h.assert_eq[String](map("")?("a;#")?, "e")

class \nodoc\ iso _TestIniParseCommentMarkerAtValueIndexZeroLiteral is UnitTest
  fun name(): String => "ini/ParseCommentMarkerAtValueIndexZeroLiteral"

  fun apply(h: TestHelper) ? =>
    // The marker may be at index 0 with no whitespace between it and `=`,
    // OR there may be whitespace which then gets trimmed away — either way,
    // the marker ends up at index 0 of the trimmed value and is kept literal.
    let input: Array[String] = [ as String:
      "k1=;literal"
      "k2=#literal"
      "k3=;"
      "k4=#"
      "k5=    ;trimmed-then-literal"
      "k6=\t#trimmed-then-literal" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("k1")?, ";literal")
    h.assert_eq[String](map("")?("k2")?, "#literal")
    h.assert_eq[String](map("")?("k3")?, ";")
    h.assert_eq[String](map("")?("k4")?, "#")
    h.assert_eq[String](map("")?("k5")?, ";trimmed-then-literal")
    h.assert_eq[String](map("")?("k6")?, "#trimmed-then-literal")

class \nodoc\ iso _TestIniParseDelimiterSelection is UnitTest
  fun name(): String => "ini/ParseDelimiterSelection"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "a:b=c"
      "p:q=r:s"
      "x:y"
      "only:colon=here" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("a:b")?, "c")
    h.assert_eq[String](map("")?("p:q")?, "r:s")
    h.assert_eq[String](map("")?("x")?, "y")
    h.assert_eq[String](map("")?("only:colon")?, "here")

class \nodoc\ iso _TestIniParseDuplicateKeyLastWins is UnitTest
  fun name(): String => "ini/ParseDuplicateKeyLastWins"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[s]"
      "k=first"
      "k=second"
      "k=third" ]
    let map = IniParse(input.values())?
    h.assert_eq[USize](map("s")?.size(), 1)
    h.assert_eq[String](map("s")?("k")?, "third")

class \nodoc\ iso _TestIniParseEmptyBrackets is UnitTest
  fun name(): String => "ini/ParseEmptyBrackets"

  fun apply(h: TestHelper) ? =>
    // The active section before `[]` is `s1`; after `[]` it must be `""`.
    // If `[]` were silently dropped, `k2` would land in `s1`.
    let input: Array[String] = [ as String:
      "[s1]"
      "k1=v1"
      "[]"
      "k2=v2" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("s1")?("k1")?, "v1")
    h.assert_eq[String](map("")?("k2")?, "v2")
    h.assert_false(map("s1")?.contains("k2"))

class \nodoc\ iso _TestIniParseEmptyInput is UnitTest
  fun name(): String => "ini/ParseEmptyInput"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = Array[String]
    let map = IniParse(input.values())?
    h.assert_eq[USize](map.size(), 0)

class \nodoc\ iso _TestIniParseEmptyKeyAndValueAllowed is UnitTest
  fun name(): String => "ini/ParseEmptyKeyAndValueAllowed"

  fun apply(h: TestHelper) =>
    // Empty key only.
    try
      let map = IniParse([as String: "=value"].values())?
      h.assert_eq[String](map("")?("")?, "value")
    else
      h.fail("=value parse raised")
    end

    // Empty value only.
    try
      let map = IniParse([as String: "key="].values())?
      h.assert_eq[String](map("")?("key")?, "")
    else
      h.fail("key= parse raised")
    end

    // Both empty.
    try
      let map = IniParse([as String: "="].values())?
      h.assert_eq[String](map("")?("")?, "")
    else
      h.fail("= parse raised")
    end

    // Colon fallback should support the same shapes.
    try
      let map = IniParse([as String: ":value"].values())?
      h.assert_eq[String](map("")?("")?, "value")
    else
      h.fail(":value parse raised")
    end

    try
      let map = IniParse([as String: "key:"].values())?
      h.assert_eq[String](map("")?("key")?, "")
    else
      h.fail("key: parse raised")
    end

    try
      let map = IniParse([as String: ":"].values())?
      h.assert_eq[String](map("")?("")?, "")
    else
      h.fail(": parse raised")
    end

class \nodoc\ iso _TestIniParseEqualsInValueKept is UnitTest
  fun name(): String => "ini/ParseEqualsInValueKept"

  fun apply(h: TestHelper) ? =>
    let map = IniParse([as String: "k=a=b=c"].values())?
    h.assert_eq[String](map("")?("k")?, "a=b=c")

class \nodoc\ iso _TestIniParseFirstClosingBracketWins is UnitTest
  fun name(): String => "ini/ParseFirstClosingBracketWins"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[a]b]c"
      "k=v" ]
    let map = IniParse(input.values())?
    h.assert_true(map.contains("a"))
    h.assert_false(map.contains("a]b]c"))
    h.assert_eq[String](map("a")?("k")?, "v")

class \nodoc\ iso _TestIniParseFullLineCommentsSkipped is UnitTest
  fun name(): String => "ini/ParseFullLineCommentsSkipped"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "; one"
      "   ; two"
      "\t# three"
      "# four"
      "k=v" ]
    let map = IniParse(input.values())?
    h.assert_eq[USize](map.size(), 1)
    h.assert_eq[USize](map("")?.size(), 1)
    h.assert_eq[String](map("")?("k")?, "v")

class \nodoc\ iso _TestIniParseGlobalSectionForPreHeaderKeys is UnitTest
  fun name(): String => "ini/ParseGlobalSectionForPreHeaderKeys"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "k1=v1"
      "k2=v2"
      "[s]"
      "k3=v3" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("k1")?, "v1")
    h.assert_eq[String](map("")?("k2")?, "v2")
    h.assert_eq[String](map("s")?("k3")?, "v3")
    h.assert_false(map("s")?.contains("k1"))
    h.assert_false(map("s")?.contains("k2"))

class \nodoc\ iso _TestIniParseInlineCommentInsideQuotedLooking is UnitTest
  fun name(): String => "ini/ParseInlineCommentInsideQuotedLooking"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "a=\"x ; y\""
      "b=\"x ;\""
      "c=\"x # y\"" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("a")?, "\"x")
    h.assert_eq[String](map("")?("b")?, "\"x")
    h.assert_eq[String](map("")?("c")?, "\"x")

class \nodoc\ iso _TestIniParseInlineCommentRequiresSpaceOrTab is UnitTest
  fun name(): String => "ini/ParseInlineCommentRequiresSpaceOrTab"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "k1=val ; c"
      "k2=val\t; c"
      "k3=val;c"
      "k4=val#c" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("k1")?, "val")
    h.assert_eq[String](map("")?("k2")?, "val")
    h.assert_eq[String](map("")?("k3")?, "val;c")
    h.assert_eq[String](map("")?("k4")?, "val#c")

class \nodoc\ iso _TestIniParseInternalKeyAndValueWhitespacePreserved is
  UnitTest
  fun name(): String => "ini/ParseInternalKeyAndValueWhitespacePreserved"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] =
      [as String: "key with spaces = value with spaces"]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("key with spaces")?, "value with spaces")

class \nodoc\ iso _TestIniParseKeyAndValueIndividuallyStripped is UnitTest
  fun name(): String => "ini/ParseKeyAndValueIndividuallyStripped"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "   key   =   value   "
      "k\t=\tv" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("key")?, "value")
    h.assert_eq[String](map("")?("k")?, "v")

class \nodoc\ iso _TestIniParseLooksLikeSectionButIsnt is UnitTest
  fun name(): String => "ini/ParseLooksLikeSectionButIsnt"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "]foo=bar"
      "foo[bar]=baz" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("]foo")?, "bar")
    h.assert_eq[String](map("")?("foo[bar]")?, "baz")

class \nodoc\ iso _TestIniParseNestedSectionNotInterpreted is UnitTest
  fun name(): String => "ini/ParseNestedSectionNotInterpreted"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[a.b]"
      "k=v" ]
    let map = IniParse(input.values())?
    h.assert_true(map.contains("a.b"))
    h.assert_false(map.contains("a"))
    h.assert_eq[String](map("a.b")?("k")?, "v")

class \nodoc\ iso _TestIniParseQuoteCharsLiteral is UnitTest
  fun name(): String => "ini/ParseQuoteCharsLiteral"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "a=\"hello\""
      "b='world'"
      "c=a\\b\\c" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("a")?, "\"hello\"")
    h.assert_eq[String](map("")?("b")?, "'world'")
    h.assert_eq[String](map("")?("c")?, "a\\b\\c")
    h.assert_eq[USize](map("")?("c")?.size(), 5)

class \nodoc\ iso _TestIniParseRaisesOnIncompleteSection is UnitTest
  fun name(): String => "ini/ParseRaisesOnIncompleteSection"

  fun apply(h: TestHelper) =>
    h.assert_true(_raises([as String: "["]), "bare [ should raise")
    h.assert_true(_raises([as String: "[abc"]), "[abc should raise")
    h.assert_false(_raises([as String: "[abc]"]), "[abc] should not raise")

  fun _raises(input: Array[String]): Bool =>
    try
      IniParse(input.values())?
      false
    else
      true
    end

class \nodoc\ iso _TestIniParseRaisesOnNoDelimiter is UnitTest
  fun name(): String => "ini/ParseRaisesOnNoDelimiter"

  fun apply(h: TestHelper) =>
    h.assert_true(_raises([as String: "abc"]), "abc should raise")
    h.assert_true(_raises([as String: "]"]), "] should raise")
    h.assert_true(_raises([as String: "a b c"]), "a b c should raise")
    h.assert_false(_raises([as String: "a="]), "a= should not raise")

  fun _raises(input: Array[String]): Bool =>
    try
      IniParse(input.values())?
      false
    else
      true
    end

class \nodoc\ iso _TestIniParseRepeatedSectionMerges is UnitTest
  fun name(): String => "ini/ParseRepeatedSectionMerges"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[s]"
      "k1=v1"
      "[other]"
      "x=y"
      "[s]"
      "k2=v2" ]
    let map = IniParse(input.values())?
    h.assert_eq[USize](map("s")?.size(), 2)
    h.assert_eq[String](map("s")?("k1")?, "v1")
    h.assert_eq[String](map("s")?("k2")?, "v2")
    h.assert_eq[String](map("other")?("x")?, "y")

class \nodoc\ iso _TestIniParseSectionEmptyMap is UnitTest
  fun name(): String => "ini/ParseSectionEmptyMap"

  fun apply(h: TestHelper) ? =>
    let map = IniParse([as String: "[s]"].values())?
    h.assert_true(map.contains("s"))
    h.assert_eq[USize](map("s")?.size(), 0)

class \nodoc\ iso _TestIniParseSectionHeaderBeatsKv is UnitTest
  fun name(): String => "ini/ParseSectionHeaderBeatsKv"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[a=b]"
      "k=v" ]
    let map = IniParse(input.values())?
    h.assert_true(map.contains("a=b"))
    h.assert_eq[String](map("a=b")?("k")?, "v")
    // The `=` inside the brackets is part of the section name; no k/v pair
    // is emitted for the section-header line, so the implicit global section
    // never gets created.
    h.assert_false(map.contains(""))

class \nodoc\ iso _TestIniParseSectionNameWithDelimitersAndCommentChars is
  UnitTest
  fun name(): String => "ini/ParseSectionNameWithDelimitersAndCommentChars"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[a=b:c;d#e]"
      "k=v" ]
    let map = IniParse(input.values())?
    h.assert_true(map.contains("a=b:c;d#e"))
    h.assert_eq[String](map("a=b:c;d#e")?("k")?, "v")

class \nodoc\ iso _TestIniParseSemicolonShadowsLaterComment is UnitTest
  fun name(): String => "ini/ParseSemicolonShadowsLaterComment"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "k1=a;b ; c"
      "k2=a;b # c"
      "k3=a ;b # c"
      "k4=a # b" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("k1")?, "a;b ; c")
    h.assert_eq[String](map("")?("k2")?, "a;b # c")
    // k3: first `;` is preceded by a space, so it IS a comment; the trailing
    // `# c` is part of what gets discarded.
    h.assert_eq[String](map("")?("k3")?, "a")
    h.assert_eq[String](map("")?("k4")?, "a")

class \nodoc\ iso _TestIniParseSingleCharLines is UnitTest
  fun name(): String => "ini/ParseSingleCharLines"

  fun apply(h: TestHelper) =>
    // ; is a comment — parses, empty map.
    _expect_ok(h, [as String: ";"], 0)
    // # is a comment — parses, empty map.
    _expect_ok(h, [as String: "#"], 0)
    // = is empty key, empty value.
    try
      let map = IniParse([as String: "="].values())?
      h.assert_eq[String](map("")?("")?, "")
    else
      h.fail("= should not raise")
    end
    // : is empty key, empty value.
    try
      let map = IniParse([as String: ":"].values())?
      h.assert_eq[String](map("")?("")?, "")
    else
      h.fail(": should not raise")
    end
    // [ raises (IniIncompleteSection).
    h.assert_true(_raises([as String: "["]), "[ should raise")
    // ] raises (IniNoDelimiter).
    h.assert_true(_raises([as String: "]"]), "] should raise")
    // a raises (IniNoDelimiter).
    h.assert_true(_raises([as String: "a"]), "a should raise")

  fun _expect_ok(h: TestHelper, input: Array[String], expected_size: USize) =>
    try
      let map = IniParse(input.values())?
      h.assert_eq[USize](map.size(), expected_size)
    else
      h.fail("input should not raise")
    end

  fun _raises(input: Array[String]): Bool =>
    try
      IniParse(input.values())?
      false
    else
      true
    end

class \nodoc\ iso _TestIniParseTextAfterClosingBracketDiscarded is UnitTest
  fun name(): String => "ini/ParseTextAfterClosingBracketDiscarded"

  fun apply(h: TestHelper) ? =>
    // Plain trailing text.
    let map1 = IniParse([as String: "[a] trailing junk"; "k=v"].values())?
    h.assert_true(map1.contains("a"))
    h.assert_eq[String](map1("a")?("k")?, "v")

    // Trailing comment-looking text.
    let map2 = IniParse([as String: "[a]; not a comment"; "k=v"].values())?
    h.assert_true(map2.contains("a"))
    h.assert_eq[String](map2("a")?("k")?, "v")

    // Trailing text that contains `=` — must not parse as a k/v.
    let map3 = IniParse([as String: "[a]=b"; "k=v"].values())?
    h.assert_true(map3.contains("a"))
    h.assert_eq[String](map3("a")?("k")?, "v")
    h.assert_false(map3.contains(""))

class \nodoc\ iso _TestIniParseTrailingWhitespaceBeforeCommentStripped is
  UnitTest
  fun name(): String => "ini/ParseTrailingWhitespaceBeforeCommentStripped"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "k1=val   ;c"
      "k2=val\t\t;c" ]
    let map = IniParse(input.values())?
    h.assert_eq[String](map("")?("k1")?, "val")
    h.assert_eq[String](map("")?("k2")?, "val")

class \nodoc\ iso _TestIniParseUtf8Passthrough is UnitTest
  fun name(): String => "ini/ParseUtf8Passthrough"

  fun apply(h: TestHelper) ? =>
    let input: Array[String] = [ as String:
      "[café]"
      "clé=valeur" ]
    let map = IniParse(input.values())?
    h.assert_true(map.contains("café"))
    h.assert_eq[String](map("café")?("clé")?, "valeur")

class \nodoc\ iso _TestIniPropertyCommentLineRecognition is UnitTest
  // For any leading whitespace + `;` or `#` + arbitrary printable tail (with
  // `;` and `#` excluded from the tail to keep the rule unambiguous), the
  // line is treated as a comment. The control line `k=v` is included so the
  // assertion would fail if the parser silently dropped every line.
  fun name(): String => "ini/PropertyCommentLineRecognition"

  fun apply(h: TestHelper) ? =>
    let rand = Rand(0x1234567890abcdef, 0xfedcba0987654321)
    var iter: USize = 0
    while iter < 200 do
      let ws_len = rand.int_unbiased[USize](7)
      let prefix = recover trn String end
      var i: USize = 0
      while i < ws_len do
        prefix.push(if (rand.next() and 1) == 0 then ' ' else '\t' end)
        i = i + 1
      end
      let marker: U8 = if (rand.next() and 1) == 0 then ';' else '#' end
      prefix.push(marker)
      let tail_len = rand.int_unbiased[USize](41)
      var j: USize = 0
      while j < tail_len do
        var b: U8 = rand.int_unbiased[U64](95).u8() + 0x20
        if (b == ';') or (b == '#') then b = 'x' end
        prefix.push(b)
        j = j + 1
      end
      let line: String val = consume prefix
      let input: Array[String] = [as String: line; "k=v"]
      let map = IniParse(input.values())?
      h.assert_eq[USize](map.size(), 1,
        "expected only the control entry; line=" + line)
      h.assert_eq[String](map("")?("k")?, "v",
        "control entry missing; line=" + line)
      iter = iter + 1
    end

class \nodoc\ iso _TestIniPropertyLineLevelTrimInvariance is UnitTest
  // Whitespace at four positions — before the key, between the key and `=`,
  // between `=` and the value, and after the value — is trimmed away. The
  // result is always ("key", "value"). The leading and trailing line-level
  // whitespace is handled by the line strip OR by the inner key/value
  // strips; the inner whitespace (between key and `=`, and between `=` and
  // value) requires the inner strips. The line-level strip in isolation is
  // pinned by `_TestIniParseFullLineCommentsSkipped` and
  // `_TestIniParseBlankLinesIgnored`.
  fun name(): String => "ini/PropertyLineLevelTrimInvariance"

  fun apply(h: TestHelper) ? =>
    let rand = Rand(0x0fedcba987654321, 0x123456789abcdef0)
    var iter: USize = 0
    while iter < 100 do
      let line = recover trn String end
      line.append(_ws(rand, 4))
      line.append("key")
      line.append(_ws(rand, 4))
      line.push('=')
      line.append(_ws(rand, 4))
      line.append("value")
      line.append(_ws(rand, 4))
      let final_line: String val = consume line
      let map = IniParse([as String: final_line].values())?
      h.assert_eq[String](map("")?("key")?, "value",
        "trim invariance failed; line=" + final_line)
      iter = iter + 1
    end

  fun _ws(rand: Rand, max_len: USize): String =>
    let len = rand.int_unbiased[USize](max_len + 1)
    let s = recover trn String end
    var i: USize = 0
    while i < len do
      s.push(if (rand.next() and 1) == 0 then ' ' else '\t' end)
      i = i + 1
    end
    consume s

class \nodoc\ iso _TestIniStreamingAddSectionHaltsAfterErrorReturnsFalse is
  UnitTest
  fun name(): String =>
    "ini/StreamingAddSectionHaltsAfterErrorReturnsFalse"

  fun apply(h: TestHelper) =>
    let n = _RecordingNotify
    n.errors_returns = true
    n.add_section_returns = false
    let input: Array[String] = [as String: "abc"; "[s]"]
    let ok = Ini(input.values(), n)
    h.assert_false(ok)
    h.assert_eq[USize](n.errs.size(), 1)
    h.assert_eq[USize](n.sections.size(), 1)

class \nodoc\ iso _TestIniStreamingAddSectionHaltsCleanReturnsTrue is UnitTest
  fun name(): String => "ini/StreamingAddSectionHaltsCleanReturnsTrue"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    n.add_section_returns = false
    let input: Array[String] = [as String: "[s1]"; "[s2]"]
    let ok = Ini(input.values(), n)
    h.assert_true(ok)
    h.assert_eq[USize](n.sections.size(), 1)
    h.assert_eq[String](n.sections(0)?, "s1")
    h.assert_eq[USize](n.kvs.size(), 0)

class \nodoc\ iso _TestIniStreamingApplyHaltsAfterErrorReturnsFalse is UnitTest
  fun name(): String => "ini/StreamingApplyHaltsAfterErrorReturnsFalse"

  fun apply(h: TestHelper) =>
    let n = _RecordingNotify
    n.errors_returns = true
    n.apply_returns = false
    let input: Array[String] = [as String: "abc"; "k=v"; "k2=v2"]
    let ok = Ini(input.values(), n)
    h.assert_false(ok)
    h.assert_eq[USize](n.errs.size(), 1)
    h.assert_eq[USize](n.kvs.size(), 1)

class \nodoc\ iso _TestIniStreamingApplyHaltsCleanReturnsTrue is UnitTest
  fun name(): String => "ini/StreamingApplyHaltsCleanReturnsTrue"

  fun apply(h: TestHelper) =>
    let n = _RecordingNotify
    n.apply_returns = false
    let input: Array[String] = [as String: "a=1"; "b=2"; "c=3"]
    let ok = Ini(input.values(), n)
    h.assert_true(ok)
    h.assert_eq[USize](n.kvs.size(), 1)

class \nodoc\ iso _TestIniStreamingBareOpenBracketReportsIncompleteSection is
  UnitTest
  fun name(): String => "ini/StreamingBareOpenBracketReportsIncompleteSection"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    let input: Array[String] = [as String: "["]
    let ok = Ini(input.values(), n)
    h.assert_false(ok)
    h.assert_eq[USize](n.errs.size(), 1)
    (let line, let err) = n.errs(0)?
    h.assert_eq[USize](line, 1)
    h.assert_true(err is IniIncompleteSection)

class \nodoc\ iso _TestIniStreamingCallOrderPreservesInputOrder is UnitTest
  fun name(): String => "ini/StreamingCallOrderPreservesInputOrder"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    let input: Array[String] = [as String:
      "a=1"
      "[s1]"
      "b=2"
      "[s2]"
      "c=3" ]
    let ok = Ini(input.values(), n)
    h.assert_true(ok)
    h.assert_eq[USize](n.sections.size(), 2)
    h.assert_eq[String](n.sections(0)?, "s1")
    h.assert_eq[String](n.sections(1)?, "s2")
    h.assert_eq[USize](n.kvs.size(), 3)
    (let s0, let k0, let v0) = n.kvs(0)?
    h.assert_eq[String](s0, "")
    h.assert_eq[String](k0, "a")
    h.assert_eq[String](v0, "1")
    (let s1, let k1, let v1) = n.kvs(1)?
    h.assert_eq[String](s1, "s1")
    h.assert_eq[String](k1, "b")
    h.assert_eq[String](v1, "2")
    (let s2, let k2, let v2) = n.kvs(2)?
    h.assert_eq[String](s2, "s2")
    h.assert_eq[String](k2, "c")
    h.assert_eq[String](v2, "3")

class \nodoc\ iso _TestIniStreamingCaseSensitiveVerbatim is UnitTest
  // The parser itself does no case-folding. This test asserts the section
  // and key strings passed to the notifier are byte-for-byte what was on the
  // input — confirming the case-sensitivity guarantee in `IniParse` is
  // upstream of the `Map`, not a property of the `Map` alone.
  fun name(): String => "ini/StreamingCaseSensitiveVerbatim"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    let input: Array[String] = [as String:
      "[Section]"
      "Key=A"
      "[section]"
      "key=B" ]
    let ok = Ini(input.values(), n)
    h.assert_true(ok)
    h.assert_eq[USize](n.sections.size(), 2)
    h.assert_eq[String](n.sections(0)?, "Section")
    h.assert_eq[String](n.sections(1)?, "section")
    h.assert_eq[USize](n.kvs.size(), 2)
    (_, let k0, _) = n.kvs(0)?
    h.assert_eq[String](k0, "Key")
    (_, let k1, _) = n.kvs(1)?
    h.assert_eq[String](k1, "key")

class \nodoc\ iso _TestIniStreamingDefaultErrorsContinuesParsing is UnitTest
  // Confirms `IniNotify`'s trait defaults are wired correctly. A minimal
  // notifier that only overrides `apply` must (a) continue past an error
  // (`errors` default returns `true`) and (b) accept section headers without
  // halting (`add_section` default returns `true`). The input below mixes
  // an error line, a section header, and a key/value pair to exercise both
  // defaults.
  fun name(): String => "ini/StreamingDefaultErrorsContinuesParsing"

  fun apply(h: TestHelper) =>
    let recorder: Array[(String, String, String)] = recorder.create()
    let n = object ref is IniNotify
      let r: Array[(String, String, String)] = recorder
      fun ref apply(section: String, key: String, value: String): Bool =>
        r.push((section, key, value))
        true
    end
    let input: Array[String] = [as String: "abc"; "[s]"; "k=v"]
    let ok = Ini(input.values(), n)
    h.assert_false(ok)
    h.assert_eq[USize](recorder.size(), 1)
    try
      (let s, _, _) = recorder(0)?
      h.assert_eq[String](s, "s")
    else
      h.fail("recorder is empty")
    end

class \nodoc\ iso _TestIniStreamingDuplicateKeyFiresAll is UnitTest
  // The parser fires three apply calls for three same-key lines. Pins the
  // dispatch behavior independently of `IniParse`'s map overwrite.
  fun name(): String => "ini/StreamingDuplicateKeyFiresAll"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    let input: Array[String] = [as String:
      "[s]"
      "k=first"
      "k=second"
      "k=third" ]
    let ok = Ini(input.values(), n)
    h.assert_true(ok)
    h.assert_eq[USize](n.kvs.size(), 3)
    (_, _, let v0) = n.kvs(0)?
    (_, _, let v1) = n.kvs(1)?
    (_, _, let v2) = n.kvs(2)?
    h.assert_eq[String](v0, "first")
    h.assert_eq[String](v1, "second")
    h.assert_eq[String](v2, "third")

class \nodoc\ iso _TestIniStreamingEmptyIteratorReturnsTrue is UnitTest
  fun name(): String => "ini/StreamingEmptyIteratorReturnsTrue"

  fun apply(h: TestHelper) =>
    let n = _RecordingNotify
    let input: Array[String] = Array[String]
    let ok = Ini(input.values(), n)
    h.assert_true(ok)
    h.assert_eq[USize](n.kvs.size(), 0)
    h.assert_eq[USize](n.sections.size(), 0)
    h.assert_eq[USize](n.errs.size(), 0)

class \nodoc\ iso _TestIniStreamingErrorsContinueButFinalFalse is UnitTest
  fun name(): String => "ini/StreamingErrorsContinueButFinalFalse"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    n.errors_returns = true
    let input: Array[String] = [as String:
      "[broken"
      "good=ok"
      "alsoBad" ]
    let ok = Ini(input.values(), n)
    h.assert_false(ok)
    h.assert_eq[USize](n.errs.size(), 2)
    (let l0, let e0) = n.errs(0)?
    h.assert_eq[USize](l0, 1)
    h.assert_true(e0 is IniIncompleteSection)
    (let l1, let e1) = n.errs(1)?
    h.assert_eq[USize](l1, 3)
    h.assert_true(e1 is IniNoDelimiter)
    h.assert_eq[USize](n.kvs.size(), 1)

class \nodoc\ iso _TestIniStreamingErrorsHaltStopsImmediately is UnitTest
  fun name(): String => "ini/StreamingErrorsHaltStopsImmediately"

  fun apply(h: TestHelper) =>
    let n = _RecordingNotify
    n.errors_returns = false
    let input: Array[String] = [as String: "[broken"; "good=ok"]
    let ok = Ini(input.values(), n)
    h.assert_false(ok)
    h.assert_eq[USize](n.errs.size(), 1)
    h.assert_eq[USize](n.kvs.size(), 0)
    h.assert_eq[USize](n.sections.size(), 0)

class \nodoc\ iso _TestIniStreamingIncompleteSectionDoesNotChangeCurrentSection
  is UnitTest
  // When a `[`-line errors and parsing continues, the parser's current
  // section is unchanged. Subsequent k/v pairs land in the previously-active
  // section.
  fun name(): String =>
    "ini/StreamingIncompleteSectionDoesNotChangeCurrentSection"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    n.errors_returns = true
    let input: Array[String] = [as String:
      "[good]"
      "k1=v1"
      "[bad"
      "k2=v2" ]
    let ok = Ini(input.values(), n)
    h.assert_false(ok)
    h.assert_eq[USize](n.kvs.size(), 2)
    (let s0, _, _) = n.kvs(0)?
    (let s1, _, _) = n.kvs(1)?
    h.assert_eq[String](s0, "good")
    h.assert_eq[String](s1, "good")

class \nodoc\ iso _TestIniStreamingLineNumbersOneBased is UnitTest
  fun name(): String => "ini/StreamingLineNumbersOneBased"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    n.errors_returns = true
    let input: Array[String] = [as String:
      ""
      "; comment"
      "[bad"
      ""
      "alsoBad" ]
    let ok = Ini(input.values(), n)
    h.assert_false(ok)
    h.assert_eq[USize](n.errs.size(), 2)
    (let l0, let e0) = n.errs(0)?
    h.assert_eq[USize](l0, 3)
    h.assert_true(e0 is IniIncompleteSection)
    (let l1, let e1) = n.errs(1)?
    h.assert_eq[USize](l1, 5)
    h.assert_true(e1 is IniNoDelimiter)

class \nodoc\ iso _TestIniStreamingNoAddSectionForImplicitGlobal is UnitTest
  // Keys before any `[section]` header are reported with `section=""` but no
  // synthetic `add_section("")` is dispatched.
  fun name(): String => "ini/StreamingNoAddSectionForImplicitGlobal"

  fun apply(h: TestHelper) ? =>
    let n = _RecordingNotify
    let input: Array[String] = [as String: "k=v"]
    let ok = Ini(input.values(), n)
    h.assert_true(ok)
    h.assert_eq[USize](n.sections.size(), 0)
    h.assert_eq[USize](n.kvs.size(), 1)
    (let s0, _, _) = n.kvs(0)?
    h.assert_eq[String](s0, "")

class \nodoc\ iso _TestIniStreamingNoDelimiterReports is UnitTest
  fun name(): String => "ini/StreamingNoDelimiterReports"

  fun apply(h: TestHelper) ? =>
    // Plain bareword line.
    let n1 = _RecordingNotify
    let ok1 = Ini([as String: "abc"].values(), n1)
    h.assert_false(ok1)
    h.assert_eq[USize](n1.errs.size(), 1)
    (let line1, let err1) = n1.errs(0)?
    h.assert_eq[USize](line1, 1)
    h.assert_true(err1 is IniNoDelimiter)

    // `]` line — also IniNoDelimiter on the streaming path.
    let n2 = _RecordingNotify
    let ok2 = Ini([as String: "]"].values(), n2)
    h.assert_false(ok2)
    h.assert_eq[USize](n2.errs.size(), 1)
    (let line2, let err2) = n2.errs(0)?
    h.assert_eq[USize](line2, 1)
    h.assert_true(err2 is IniNoDelimiter)
