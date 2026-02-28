use "pony_test"
use "pony_check"
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
    test(_TestRegistryASTRules)

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
    test(_TestCommentSpacingDocstringURL)
    test(_TestCommentSpacingAfterDocstring)
    test(_TestCommentSpacingAfterCode)
    test(_TestCommentSpacingAfterCodeBadSpacing)
    test(_TestCommentSpacingNoComment)
    test(_TestCommentSpacingProperty)

    // GlobMatch tests
    test(_TestGlobMatchLiteral)
    test(_TestGlobMatchStar)
    test(_TestGlobMatchDoubleStarLeading)
    test(_TestGlobMatchDoubleStarTrailing)
    test(_TestGlobMatchDoubleStarMiddle)
    test(_TestGlobMatchDoubleStarNotComponent)
    test(_TestGlobMatchDoubleStarAlone)
    test(_TestGlobMatchEdgeCases)
    test(Property1UnitTest[String](
      _TestGlobMatchLiteralSelfMatchProperty))
    test(Property1UnitTest[String](
      _TestGlobMatchStarNoCrossSlashProperty))
    test(Property1UnitTest[String](
      _TestGlobMatchDoubleStarMatchesAllProperty))

    // PatternParser tests
    test(_TestPatternParserBlankLine)
    test(_TestPatternParserComment)
    test(_TestPatternParserNegation)
    test(_TestPatternParserTrailingSlash)
    test(_TestPatternParserLeadingSlash)
    test(_TestPatternParserAnchoring)
    test(_TestPatternParserEscapes)
    test(_TestPatternParserTrailingWhitespace)
    test(_TestPatternParserSimpleWildcard)
    test(_TestPatternParserDoubleStarAnchored)

    // IgnoreMatcher tests
    test(_TestIgnoreMatcherGitignore)
    test(_TestIgnoreMatcherIgnoreFile)
    test(_TestIgnoreMatcherIgnoreOverridesGitignore)
    test(_TestIgnoreMatcherNegation)
    test(_TestIgnoreMatcherNonGitIgnoresGitignore)
    test(_TestIgnoreMatcherHierarchical)
    test(_TestIgnoreMatcherAnchoredPattern)

    // Linter tests
    test(_TestLinterSingleFile)
    test(_TestLinterCleanFile)
    test(_TestLinterSkipsCorral)
    test(_TestLinterSuppressedDiagnosticsFiltered)
    test(_TestLinterDiagnosticsSorted)
    test(_TestLinterNonExistentTarget)
    test(_TestLinterRespectsGitignore)
    test(_TestLinterExplicitFileBypassesIgnore)
    test(_TestLinterRespectsGitignoreFromParent)

    // ExplainHelpers tests
    test(_TestExplainFormatEnabled)
    test(_TestExplainFormatDisabled)
    test(_TestExplainURLConstruction)

    // NamingHelpers tests
    test(_TestIsCamelCaseValid)
    test(_TestIsCamelCaseInvalid)
    test(_TestIsSnakeCaseValid)
    test(_TestIsSnakeCaseInvalid)
    test(_TestHasLoweredAcronym)
    test(_TestToSnakeCase)
    test(Property1UnitTest[String](
      _TestCamelCaseValidProperty))
    test(Property1UnitTest[String](
      _TestCamelCaseInvalidProperty))
    test(Property1UnitTest[String](
      _TestSnakeCaseValidProperty))
    test(Property1UnitTest[String](
      _TestSnakeCaseInvalidProperty))

    // TypeNaming tests
    test(_TestTypeNamingClean)
    test(_TestTypeNamingViolation)
    test(_TestTypeNamingPrivate)
    test(_TestTypeNamingAllEntityTypes)

    // MemberNaming tests
    test(_TestMemberNamingClean)
    test(_TestMemberNamingMethodViolation)
    test(_TestMemberNamingFieldViolation)
    test(_TestMemberNamingDontcareSkipped)
    test(_TestMemberNamingParamViolation)

    // AcronymCasing tests
    test(_TestAcronymCasingClean)
    test(_TestAcronymCasingViolation)
    test(_TestAcronymCasingHTTP)

    // FileNaming tests
    test(_TestFileNamingSingleEntity)
    test(_TestFileNamingMatchingName)
    test(_TestFileNamingTraitPrincipal)
    test(_TestFileNamingMultipleEntitiesSkipped)
    test(_TestFileNamingTestPonyMain)
    test(_TestFileNamingPrivateType)

    // PackageNaming tests
    test(_TestPackageNamingClean)
    test(_TestPackageNamingViolation)
    test(_TestPackageNamingSimpleName)

    // PublicDocstring tests
    test(_TestPublicDocstringTypeClean)
    test(_TestPublicDocstringTypeViolation)
    test(_TestPublicDocstringMethodClean)
    test(_TestPublicDocstringMethodViolation)
    test(_TestPublicDocstringPrivateSkipped)
    test(_TestPublicDocstringExemptMethods)
    test(_TestPublicDocstringAllEntityTypes)
    test(_TestPublicDocstringBehavior)
    test(_TestPublicDocstringTypeAlias)
    test(_TestPublicDocstringSimpleBody)
    test(_TestPublicDocstringAbstractMethod)
    test(_TestPublicDocstringNonExemptConstructor)
    test(_TestPublicDocstringNodocTypeSkipped)
    test(_TestPublicDocstringNodocMethodSkipped)
    test(_TestPublicDocstringNodocEntityMethodsSkipped)
    test(_TestPublicDocstringMainActorSkipped)

    // MatchSingleLine tests
    test(_TestMatchSingleLineClean)
    test(_TestMatchSingleLineViolation)
    test(_TestMatchSingleLineMultiLineBody)
    test(_TestMatchSingleLineElseSeparateLine)

    // MatchCaseIndent tests
    test(_TestMatchCaseIndentClean)
    test(_TestMatchCaseIndentViolation)
    test(_TestMatchCaseIndentNestedClean)

    // PartialSpacing tests
    test(_TestPartialSpacingClean)
    test(_TestPartialSpacingReturnTypeClean)
    test(_TestPartialSpacingNoSpaceBefore)
    test(_TestPartialSpacingNonPartialClean)

    // PartialCallSpacing tests
    test(_TestPartialCallSpacingClean)
    test(_TestPartialCallSpacingViolation)
    test(_TestPartialCallSpacingNonPartialClean)

    // DotSpacing tests
    test(_TestDotSpacingClean)
    test(_TestDotSpacingSpaceBeforeViolation)
    test(_TestDotSpacingChainNoSpaceViolation)
    test(_TestDotSpacingChainSpacedClean)
    test(_TestDotSpacingContinuationLineClean)
    test(_TestDotSpacingChainContinuationClean)

    // BlankLines tests (within-entity)
    test(_TestBlankLinesNoBlankBeforeFirstField)
    test(_TestBlankLinesBlankBeforeFirstField)
    test(_TestBlankLinesDocstringNoBlank)
    test(_TestBlankLinesMultiLineHeaderClean)
    test(_TestBlankLinesConsecutiveFieldsClean)
    test(_TestBlankLinesConsecutiveFieldsBlank)
    test(_TestBlankLinesFieldToMethodOneBlank)
    test(_TestBlankLinesFieldToMethodNoBlank)
    test(_TestBlankLinesMultiLineMethodsOneBlank)
    test(_TestBlankLinesMultiLineMethodsNoBlank)
    test(_TestBlankLinesOneLineMethodsNoBlank)

    // BlankLines tests (between-entity)
    test(_TestBlankLinesBetweenEntitiesOneBlank)
    test(_TestBlankLinesBetweenEntitiesNoBlank)
    test(_TestBlankLinesBetweenEntitiesTooMany)
    test(_TestBlankLinesOneLinerEntitiesNoBlank)
    test(_TestBlankLinesBetweenDocstringEntities)

    // IndentationSize tests
    test(_TestIndentationSizeClean)
    test(_TestIndentationSizeOddViolation)
    test(_TestIndentationSizeOneSpace)
    test(_TestIndentationSizeDocstringSkipped)
    test(_TestIndentationSizeBlankSkipped)
    test(_TestIndentationSizeZeroIndent)
    test(_TestIndentationSizeTabSkipped)

    // DocstringFormat tests
    test(_TestDocstringFormatEntityClean)
    test(_TestDocstringFormatMethodClean)
    test(_TestDocstringFormatSingleLine)
    test(_TestDocstringFormatClosingContent)
    test(_TestDocstringFormatNoDocstring)
    test(_TestDocstringFormatNodocEntityExempt)
    test(_TestDocstringFormatNodocEntityMethodExempt)

    // PackageDocstring tests
    test(_TestPackageDocstringClean)
    test(_TestPackageDocstringNoFile)
    test(_TestPackageDocstringNoDocstring)
    test(_TestPackageDocstringHyphenated)

    // OperatorSpacing tests
    test(_TestOperatorSpacingCleanBinary)
    test(_TestOperatorSpacingNoSpaces)
    test(_TestOperatorSpacingSpaceOnlyBefore)
    test(_TestOperatorSpacingSpaceOnlyAfter)
    test(_TestOperatorSpacingUnaryMinusClean)
    test(_TestOperatorSpacingUnaryMinusSpaceViolation)
    test(_TestOperatorSpacingNotClean)
    test(_TestOperatorSpacingNotAfterParenClean)
    test(_TestOperatorSpacingNotNoSpaceAfter)
    test(_TestOperatorSpacingAssignClean)
    test(_TestOperatorSpacingAssignViolation)
    test(_TestOperatorSpacingMultipleViolations)
    test(_TestOperatorSpacingKeywordBinaryClean)
    test(_TestOperatorSpacingIdentityClean)
    test(_TestOperatorSpacingSaturatingClean)
    test(_TestOperatorSpacingStyleGuideExample)
    test(_TestOperatorSpacingContinuationLineClean)

    // LambdaSpacing tests
    test(_TestLambdaSpacingSingleLineClean)
    test(_TestLambdaSpacingSingleLineMissingSpaceBefore)
    test(_TestLambdaSpacingSpaceAfterOpen)
    test(_TestLambdaSpacingMultiLineClean)
    test(_TestLambdaSpacingMultiLineSpaceBeforeClose)
    test(_TestLambdaSpacingTypeClean)
    test(_TestLambdaSpacingTypeSpaceAfterOpen)
    test(_TestLambdaSpacingTypeSpaceBeforeClose)
    test(_TestLambdaSpacingBareLambdaClean)
    test(_TestLambdaSpacingBareLambdaSpaceAfterOpen)
    test(_TestLambdaSpacingBareLambdaTypeClean)

    // AssignmentIndent tests
    test(_TestAssignmentIndentSingleLine)
    test(_TestAssignmentIndentRHSNextLine)
    test(_TestAssignmentIndentMultiLineNextLine)
    test(_TestAssignmentIndentViolation)
    test(_TestAssignmentIndentRecoverViolation)
    test(_TestAssignmentIndentReassignment)
    test(_TestAssignmentIndentMultipleViolations)

    // PreferChaining tests
    test(_TestPreferChainingViolation)
    test(_TestPreferChainingVarViolation)
    test(_TestPreferChainingAlreadyChaining)
    test(_TestPreferChainingSingleCallTrailingRef)
    test(_TestPreferChainingSingleCallRefNotLast)
    test(_TestPreferChainingNoTrailingRef)
    test(_TestPreferChainingUsedAfterCalls)
    test(_TestPreferChainingInterspersed)
    test(_TestPreferChainingNormalLet)

    // ControlStructureAlignment tests
    test(_TestCtrlAlignIfClean)
    test(_TestCtrlAlignForClean)
    test(_TestCtrlAlignWhileClean)
    test(_TestCtrlAlignTryClean)
    test(_TestCtrlAlignRepeatClean)
    test(_TestCtrlAlignWithClean)
    test(_TestCtrlAlignIfdefClean)
    test(_TestCtrlAlignSingleLineSkipped)
    test(_TestCtrlAlignNestedClean)
    test(_TestCtrlAlignElseMisaligned)
    test(_TestCtrlAlignElseifMisaligned)
    test(_TestCtrlAlignEndMisaligned)
    test(_TestCtrlAlignForEndMisaligned)
    test(_TestCtrlAlignTryElseMisaligned)
    test(_TestCtrlAlignRepeatUntilMisaligned)
    test(_TestCtrlAlignWithEndMisaligned)
    test(_TestCtrlAlignIfdefElseMisaligned)
    test(_TestCtrlAlignMultipleMisaligned)
    test(_TestCtrlAlignWhileElseMisaligned)
    test(_TestCtrlAlignForElseMisaligned)
    test(_TestCtrlAlignRepeatElseMisaligned)
    test(_TestCtrlAlignTryThenMisaligned)
