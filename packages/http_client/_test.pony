use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    // Method tests
    test(PropertyTest[String val](_PropertyValidMethodParsesCorrectly))
    test(PropertyTest[String val](_PropertyInvalidMethodReturnsNone))
    test(PropertyTest[(String val, Bool)](
      _PropertyMethodParseBoundary))

    // Headers tests
    test(PropertyTest[(String val, String val)](
      _PropertyHeadersCaseInsensitive))
    test(PropertyTest[(String val, String val, String val)](
      _PropertyHeadersSetReplaces))
    test(PropertyTest[(String val, String val, String val)](
      _PropertyHeadersAddPreserves))

    // Parser property-based tests
    test(PropertyTest[(U16, String val)](
      _PropertyValidStatusLineParsesCorrectly))
    test(PropertyTest[String val](
      _PropertyInvalidStatusLineRejected))
    test(PropertyTest[Array[(String val, String val)] ref](
      _PropertyHeadersRoundtrip))
    test(PropertyTest[USize](
      _PropertyFixedBodyDelivered))
    test(PropertyTest[Array[USize] ref](
      _PropertyChunkedBodyDelivered))
    test(PropertyTest[(String val, Bool)](
      _PropertyStatusLineBoundary))

    // Parser example-based tests
    test(_TestParserKnownGoodResponses)
    test(_TestIncrementalByteByByte)
    test(_TestSizeLimitStatusLine)
    test(_TestSizeLimitHeaders)
    test(_TestSizeLimitBody)
    test(_TestSizeLimitCloseDelimitedBody)
    test(_TestInvalidContentLength)
    test(_TestInvalidChunkSize)
    test(_TestChunkedWithTrailers)
    test(_TestHTTP10Version)
    test(_TestInvalidVersion)
    test(_TestNoBodyHead)
    test(_TestNoBody204)
    test(_TestNoBody304)
    test(_TestCloseDelimitedBody)
    test(_TestContentLengthZero)
    test(_TestContentLengthAndChunked)
    test(_TestDuplicateContentLength)
    test(_TestDataAfterError)
    test(_Test1xxSkipped)
    test(_TestMultiple1xx)

    // Serializer property-based tests
    test(PropertyTest[String val](
      _PropertySerializerContainsMethod))
    test(PropertyTest[String val](
      _PropertySerializerContainsPath))
    test(PropertyTest[String val](
      _PropertySerializerAutoHost))
    test(PropertyTest[USize](
      _PropertySerializerAutoContentLength))

    // Serializer example-based tests
    test(_TestSerializerKnownGood)
    test(_TestSerializerHostWithPort)
    test(_TestSerializerHostDefaultPort)
    test(_TestSerializerUserHostTakesPrecedence)
    test(_TestSerializerUserContentLengthTakesPrecedence)
    test(_TestSerializerNoBody)

    // Response collector property-based tests
    test(PropertyTest[Array[USize] ref](
      _PropertyCollectorChunkAccumulation))
    test(PropertyTest[U16](
      _PropertyCollectorPreservesResponseMetadata))

    // Response collector example-based tests
    test(_TestCollectorBuildCorrectness)
    test(_TestCollectorEmptyBody)
    test(_TestCollectorSingleChunk)
    test(_TestCollectorBuildWithoutResponse)

    // Percent encoder property-based tests
    test(PropertyTest[String val](
      _PropertyQueryUnreservedPassthrough))
    test(PropertyTest[U8](
      _PropertyQueryReservedEncoded))
    test(PropertyTest[USize](
      _PropertyFormSpacesToPlus))
    test(PropertyTest[(String val, String val)](
      _PropertyQueryParamsRoundtrip))

    // Percent encoder / query params example-based tests
    test(_TestQueryParamsKnownGood)
    test(_TestQueryParamsEmpty)
    test(_TestQueryParamsSpecialChars)
    test(_TestFormEncoderKnownGood)
    test(_TestFormEncoderEmpty)
    test(_TestFormEncoderSpecialChars)

    // Auth property-based tests
    test(PropertyTest[(String val, String val)](
      _PropertyBasicAuthFormat))
    test(PropertyTest[String val](
      _PropertyBearerAuthFormat))

    // Auth example-based tests
    test(_TestBasicAuthKnownGood)
    test(_TestBearerAuthKnownGood)

    // Request builder property-based tests
    test(PropertyTest[String val](
      _PropertyBuilderMethodCorrect))

    // Request builder example-based tests
    test(_TestBuilderGetBasic)
    test(_TestBuilderPostWithJSONBody)
    test(_TestBuilderPostWithFormBody)
    test(_TestBuilderQueryParams)
    test(_TestBuilderHeaders)
    test(_TestBuilderBasicAuth)
    test(_TestBuilderBearerAuth)
    test(_TestBuilderBodyNarrows)
    test(_TestBuilderDeleteWithBody)
    test(_TestBuilderNoQueryParams)

    // Response JSON example-based tests
    test(_TestResponseJSONValidObject)
    test(_TestResponseJSONValidArray)
    test(_TestResponseJSONInvalid)
    test(_TestResponseJSONEmptyBody)

    // JSON decoder property-based tests
    test(PropertyTest[String val](
      _PropertyDecodeJSONParseErrorPropagation))
    test(PropertyTest[String val](
      _PropertyDecodeJSONDecodeErrorPropagation))
    test(PropertyTest[String val](_PropertyDecodeJSONIdentityDecoder))

    // JSON decoder example-based tests
    test(_TestJSONDecoderSuccessfulDecode)
    test(_TestJSONDecoderMissingField)
    test(_TestJSONDecoderWrongType)
    test(_TestDecodeJSONValidJSON)
    test(_TestDecodeJSONInvalidJSON)
    test(_TestDecodeJSONWrongStructure)
    test(_TestJSONDecodeErrorString)

    // Multipart property-based tests
    test(PropertyTest[USize](_PropertyMultipartBodyStructure))

    // Multipart example-based tests
    test(_TestMultipartBoundaryFormat)
    test(_TestMultipartContentTypeBoundaryConsistency)
    test(_TestMultipartTextField)
    test(_TestMultipartFilePart)
    test(_TestMultipartMixed)
    test(_TestMultipartEmpty)
    test(_TestMultipartBuilderIntegration)
    test(_TestMultipartNonAsciiFilename)

    // Multipart escaping property-based tests
    test(PropertyTest[String val](
      _PropertyMultipartEscapedNamesWellFormed))

    // Multipart escaping example-based tests
    test(_TestMultipartFieldNameWithQuote)
    test(_TestMultipartFilenameWithQuote)
    test(_TestMultipartFieldNameWithBackslash)
    test(_TestMultipartFilenameWithBothSpecials)

    // Redirect tests
    test(_TestRedirectCrossOriginStripsCredentials)
    test(_TestRedirectSameOriginKeepsCredentials)
    test(_TestRedirectRefusesDowngrade)
    test(_TestRedirectMissingLocation)
    test(_TestRedirectLimitExhausted)
    test(_TestRedirectRemainingDecrements)
    test(_TestRedirect303PostBecomesGet)
    test(_TestRedirect307PreservesMethodAndBody)
    test(_TestRedirect301PostBecomesGet)
    test(_TestRedirect302PostBecomesGet)
    test(_TestRedirect308PreservesMethodAndBody)
    test(_TestRedirectNonRedirectStatus)
    test(_TestRedirectRelativeLocation)
    test(_TestRedirectUnsupportedSchemeLocation)
    test(_TestRedirectUserinfoRejected)
    test(_TestRedirectIPv6Origin)
    test(_TestRedirectEmptyPathDefaultsToSlash)
    test(_TestRedirectEmptyPathWithQuery)
    test(_TestRedirectEmptyHostRejected)
    test(_TestRedirectMixedCaseScheme)
    test(PropertyTest[(U16, String val)](
      _PropertyRedirectStripsCredentials))

    // Redirect follower tests
    test(_TestFollowerForwardsNonRedirect)
    test(_TestFollowerForwardsNonRedirectWithLastRequest)
    test(_TestFollowerInterceptsRedirect)
    test(_TestFollowerRedirectErrorForwards)
    test(_TestFollowerSuppressesBodyDuringRedirect)
    test(_TestFollowerForwardsBodyWithoutRedirect)
    test(_TestFollowerSuppressesClosedDuringRedirect)
    test(_TestFollowerForwardsClosedWithoutRedirect)
    test(_TestFollowerForwardsPassthroughCallbacks)
    test(_TestFollowerConnectedForwardsWithoutPending)
    test(_TestFollowerPendingRedirectSuppressesConnectedForward)
    test(_TestFollowerSameOriginRedirectDoesNotUseFactory)
    test(_TestFollowerCrossOriginRedirectSetsUpFactory)
    test(_TestFollowerCompleteForwardsWithoutRedirect)
    test(_TestFollowerErrorSuppressesBodyAndComplete)
