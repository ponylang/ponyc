use "pony_test"

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
    test(PropertyTest[String](_TestPctEncodePropertyUnreserved))
    test(PropertyTest[String](_TestPctEncodePropertyRoundtrip))
    test(PropertyTest[String](_TestPctEncodePropertyReservedSuperset))

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
    test(PropertyTest[String](_TestPropertyNoBracesInExpansion))
    test(PropertyTest[String](_TestPropertyUnreservedPassthrough))
    test(PropertyTest[String](_TestPropertyValidTemplatesParse))
    test(PropertyTest[String](_TestPropertyInvalidTemplatesFail))
    test(PropertyTest[(String, Bool)](
      _TestPropertyMixedTemplates))

    // Builder tests
    test(_TestBuilderSimpleExpansion)
    test(_TestBuilderListAndPairs)
    test(_TestBuilderInvalidTemplate)
    test(_TestBuilderEmptyVars)
    test(_TestBuilderChaining)
    test(PropertyTest[String](_TestPropertyBuilderMatchesExpand))
    test(PropertyTest[String](_TestPropertyBuilderInvalidFails))
