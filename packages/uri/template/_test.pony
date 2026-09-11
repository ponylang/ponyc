use "pony_test"
use "pony_check"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    // Percent encoding tests
    test(_TestPctEncodeUnreservedPassthrough)
    test(_TestPctEncodeSpecialChars)
    test(_TestPctEncodeReservedPassthrough)
    test(_TestPctEncodeMultibyteUtf8)
    test(_TestPctEncodeExistingTriplets)
    test(_TestPctEncodeMixedContent)
    test(Property1UnitTest[String](_TestPctEncodePropertyUnreserved))
    test(Property1UnitTest[String](_TestPctEncodePropertyRoundtrip))
    test(Property1UnitTest[String](_TestPctEncodePropertyReservedSuperset))

    // URI template expansion — RFC 6570 test vectors
    test(_TestSimpleExpansion)
    test(_TestReservedExpansion)
    test(_TestFragmentExpansion)
    test(_TestLabelExpansion)
    test(_TestPathSegmentExpansion)
    test(_TestPathParameterExpansion)
    test(_TestQueryExpansion)
    test(_TestQueryContinuationExpansion)

    // Parser error tests
    test(_TestParseErrorReservedOp)
    test(_TestParseErrorUnclosed)
    test(_TestParseErrorEmptyExpression)
    test(_TestParseErrorEmptyVarname)
    test(_TestParseErrorPrefixBounds)
    test(_TestParseErrorDotInVarname)
    test(_TestParseErrorUnexpectedCloseBrace)
    test(_TestParseErrorInvalidLiteralChar)
    test(_TestParseValidTemplates)
    test(_TestTemplateString)

    // Composite expansion edge cases
    test(_TestEmptyListUndefined)
    test(_TestEmptyPairsUndefined)
    test(_TestAllUndefined)
    test(_TestExplodeListQuery)
    test(_TestExplodePairsQuery)
    test(_TestExplodeListSemicolon)
    test(_TestPrefixUnicode)

    // Property-based tests
    test(Property1UnitTest[String](_TestPropertyNoBracesInExpansion))
    test(Property1UnitTest[String](_TestPropertyUnreservedPassthrough))
    test(Property1UnitTest[String](_TestPropertyValidTemplatesParse))
    test(Property1UnitTest[String](_TestPropertyInvalidTemplatesFail))
    test(Property1UnitTest[(String, Bool)](
      _TestPropertyMixedTemplates))

    // Builder tests
    test(_TestBuilderSimpleExpansion)
    test(_TestBuilderListAndPairs)
    test(_TestBuilderInvalidTemplate)
    test(_TestBuilderEmptyVars)
    test(_TestBuilderChaining)
    test(Property1UnitTest[String](_TestPropertyBuilderMatchesExpand))
    test(Property1UnitTest[String](_TestPropertyBuilderInvalidFails))
