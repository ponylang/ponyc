use "pony_test"
use "pony_check"
use template = "./template"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    // URI template expansion (uri/template subpackage)
    template.Main.make().tests(test)

    // Percent-encoding tests
    test(Property1UnitTest[String val](_PropertyPercentRoundtrip))
    test(Property1UnitTest[String val](
      _PropertyPercentEncodeOutputLegal))
    test(Property1UnitTest[String val](
      _PropertyInvalidPercentSequenceRejected))
    test(Property1UnitTest[(String val, Bool)](
      _PropertyPercentDecodeBoundary))
    test(_TestPercentEncodeKnownGood)

    // URI parsing tests
    test(Property1UnitTest[_ValidURIInput](_PropertyURIRoundtrip))
    test(Property1UnitTest[String val](_PropertyInvalidSchemeRejected))
    test(_TestParseURIKnownGood)

    // Authority parsing tests
    test(Property1UnitTest[_ValidAuthorityInput](
      _PropertyAuthorityRoundtrip))
    test(Property1UnitTest[String val](_PropertyInvalidPortRejected))
    test(Property1UnitTest[String val](_PropertyInvalidHostRejected))
    test(_TestParseURIAuthorityKnownGood)

    // Path segment tests
    test(Property1UnitTest[String val](_PropertyPathSegmentCount))
    test(Property1UnitTest[String val](_PropertyPathSegmentRoundtrip))
    test(Property1UnitTest[String val](_PropertyPathSegmentInvalidRejected))
    test(_TestPathSegmentsKnownGood)

    // Form URL-encoded tests
    test(Property1UnitTest[Array[(String val, String val)] val](
      _PropertyFormURLEncodedRoundtrip))
    test(Property1UnitTest[String val](_PropertyFormURLEncodedPlusDecodes))
    test(Property1UnitTest[String val](
      _PropertyFormURLEncodedInvalidRejected))
    test(_TestFormURLEncodedKnownGood)
    test(_TestURIQueryParams)
    test(_TestFormURLEncodedGet)
    test(_TestFormURLEncodedGetAll)
    test(_TestFormURLEncodedContains)
    test(_TestFormURLEncodedSize)

    // RemoveDotSegments tests
    test(Property1UnitTest[String val](_PropertyDotSegmentsIdempotent))
    test(Property1UnitTest[String val](_PropertyDotSegmentsNoDots))
    test(Property1UnitTest[String val](
      _PropertyDotSegmentsPreservesAbsolute))
    test(_TestRemoveDotSegmentsKnownGood)

    // ResolveURI tests
    test(Property1UnitTest[_ResolveInput](
      _PropertyResolveResultAbsolute))
    test(Property1UnitTest[_AbsoluteURIInput](
      _PropertyResolveEmptyRef))
    test(Property1UnitTest[(_AbsoluteURIInput, _AbsoluteURIInput)](
      _PropertyAbsoluteRefIgnoresBase))
    test(Property1UnitTest[_ValidURIInput](
      _PropertyNonAbsoluteBaseRejected))
    test(Property1UnitTest[_ResolveInput](_PropertyResolveRoundtrip))
    test(_TestResolveURIRFCNormal)
    test(_TestResolveURIRFCAbnormal)
    test(_TestResolveURIEdgeCases)

    // NormalizeURI tests
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeIdempotent))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeSchemeLowercase))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeHostLowercase))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeNoEncodedUnreserved))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeUppercaseHex))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeNoDotSegments))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeParseRoundtrip))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeNoDefaultPort))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeNoEmptyPathWithAuthority))
    test(Property1UnitTest[(_NormalizableURIInput, _NormalizableURIInput)](
      _PropertyNormalizeEquivalentConsistent))
    test(Property1UnitTest[String val](
      _PropertyNormalizeInvalidPercentRejected))
    test(_TestNormalizeURIKnownGood)
    test(_TestURIEquivalentKnownGood)

    // URI builder tests
    test(_TestBuildSimple)
    test(_TestBuildAllComponents)
    test(_TestBuildQueryParams)
    test(_TestBuildQueryParamEncoding)
    test(_TestBuildFromURI)
    test(_TestBuildModifyFromURI)
    test(_TestBuildIPLiteralHost)
    test(_TestBuildAutoEncode)
    test(_TestBuildPathAutoSlash)
    test(_TestBuildAppendPathSegment)
    test(_TestBuildInvalidScheme)
    test(_TestBuildInvalidIPLiteral)
    test(_TestBuildEmpty)
    test(_TestBuildQueryNoneVsEmpty)
    test(_TestBuildFragmentNoneVsEmpty)
    test(_TestBuildClearMethods)
    test(_TestBuildUserinfoAutoAuthority)
    test(_TestBuildPortAutoAuthority)
    test(_TestBuildQuerySetThenAdd)
    test(_TestBuildAuthorityNoScheme)
    test(Property1UnitTest[_ValidURIInput](_PropertyBuildFromRoundtrip))
    test(Property1UnitTest[_BuildInput](_PropertyBuildParseRoundtrip))
    test(Property1UnitTest[String val](_PropertyBuildInvalidSchemeFails))

    // IRI tests (RFC 3987)
    test(Property1UnitTest[String val](_PropertyIRIToURINoNonASCII))
    test(Property1UnitTest[String val](_PropertyURIToIRINoEncodedUcschar))
    test(Property1UnitTest[String val](_PropertyIRIToURIIdempotent))
    test(Property1UnitTest[String val](_PropertyURIToIRIIdempotent))
    test(Property1UnitTest[String val](_PropertyIRIToURIRoundtrip))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyNormalizeIRIIdempotent))
    test(Property1UnitTest[_NormalizableURIInput](
      _PropertyIRIEquivalentReflexive))
    test(Property1UnitTest[String val](
      _PropertyIRIEquivalentCrossForms))
    test(Property1UnitTest[String val](
      _PropertyIRIPercentEncodePreservesUcschar))
    test(Property1UnitTest[String val](
      _PropertyIRIPercentEncodeEncodesNonAllowed))
    test(_TestIRICharsBoundary)
    test(_TestIRIToURIKnownGood)
    test(_TestURIToIRIKnownGood)
    test(_TestNormalizeIRIKnownGood)
