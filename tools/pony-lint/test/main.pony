use "pony_test"
use lint = ".."

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Diagnostic tests
    test(_TestDiagnosticString)
    test(_TestDiagnosticStringSpecialChars)
    test(_TestDiagnosticSortByFile)
    test(_TestDiagnosticSortByLine)
    test(_TestDiagnosticSortByColumn)
    test(_TestDiagnosticEqual)
    test(_TestDiagnosticNotEqualDifferentRule)
    test(_TestExitCodes)

    // SourceFile tests
    test(_TestSourceFileSplitPreservesContent)
    test(_TestSourceFileNoCR)
    test(_TestSourceFileLineCount)
    test(_TestSourceFileEmptyContent)
    test(_TestSourceFileRelPath)
    test(_TestSourceFileCRLF)
    test(_TestSourceFileTrailingNewline)

    // Suppression tests
    test(_TestSuppressionOffOn)
    test(_TestSuppressionOffOnAll)
    test(_TestSuppressionAllow)
    test(_TestSuppressionCategory)
    test(_TestSuppressionRuleOverridesCategory)
    test(_TestSuppressionUnclosed)
    test(_TestSuppressionDuplicateOff)
    test(_TestSuppressionMalformed)
    test(_TestSuppressionMagicCommentLines)
    test(_TestSuppressionLintNotSuppressible)
    test(_TestSuppressionEmptyDirective)
    test(_TestSuppressionOverrideThenReSuppress)
    test(_TestSuppressionCategoryOnOverridesWildcardOff)
    test(_TestSuppressionWildcardOnOverridesCategoryOff)
    test(_TestSuppressionNoSpaceAfterSlashes)
    test(_TestSuppressionDoubleSpaceInDirective)

    // Config tests
    test(_TestConfigDefaultAllEnabled)
    test(_TestConfigCLIDisableRule)
    test(_TestConfigCLIDisableCategory)
    test(_TestConfigFileRuleOverride)
    test(_TestConfigFileCategoryDefault)
    test(_TestConfigCLIOverridesFile)
    test(_TestConfigParseValidJSON)
    test(_TestConfigParseMalformedJSON)
    test(_TestConfigParseInvalidStatus)
    test(_TestConfigParseNoRules)

    // Registry tests
    test(_TestRegistryDefaultAllEnabled)
    test(_TestRegistryDisableRule)
    test(_TestRegistryDisableCategory)
    test(_TestRegistryAllAlwaysComplete)

    // LineLength tests
    test(_TestLineLengthExactly80)
    test(_TestLineLengthOver80)
    test(_TestLineLengthMultiByteUTF8)
    test(_TestLineLengthEmptyLine)
    test(_TestLineLengthProperty)

    // TrailingWhitespace tests
    test(_TestTrailingWhitespaceSpace)
    test(_TestTrailingWhitespaceTab)
    test(_TestTrailingWhitespaceClean)
    test(_TestTrailingWhitespaceEmptyLine)
    test(_TestTrailingWhitespaceMultiple)
    test(_TestTrailingWhitespaceProperty)

    // HardTabs tests
    test(_TestHardTabsSingleTab)
    test(_TestHardTabsMultipleTabs)
    test(_TestHardTabsNoTabs)
    test(_TestHardTabsEmpty)
    test(_TestHardTabsProperty)

    // CommentSpacing tests
    test(_TestCommentSpacingGood)
    test(_TestCommentSpacingEmpty)
    test(_TestCommentSpacingNoSpace)
    test(_TestCommentSpacingTwoSpaces)
    test(_TestCommentSpacingInsideString)
    test(_TestCommentSpacingAfterCode)
    test(_TestCommentSpacingAfterCodeBadSpacing)
    test(_TestCommentSpacingNoComment)
    test(_TestCommentSpacingProperty)

    // Linter tests
    test(_TestLinterSingleFile)
    test(_TestLinterCleanFile)
    test(_TestLinterSkipsCorral)
    test(_TestLinterSuppressedDiagnosticsFiltered)
    test(_TestLinterDiagnosticsSorted)
    test(_TestLinterNonExistentTarget)
