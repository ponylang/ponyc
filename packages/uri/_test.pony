use "pony_test"
use template = "./template"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    // URI template expansion (uri/template subpackage)
    template.Main.make().tests(test)

    // Percent-encoding tests
    test(PropertyTest[String val](_PropertyPercentRoundtrip))
    test(PropertyTest[String val](
      _PropertyPercentEncodeOutputLegal))
    test(PropertyTest[String val](
      _PropertyInvalidPercentSequenceRejected))
    test(PropertyTest[(String val, Bool)](
      _PropertyPercentDecodeBoundary))
    test(_TestPercentEncodeKnownGood)

    // URI parsing tests
    test(PropertyTest[_ValidURIInput](_PropertyURIRoundtrip))
    test(PropertyTest[String val](_PropertyInvalidSchemeRejected))
    test(_TestParseURIKnownGood)

    // Authority parsing tests
    test(PropertyTest[_ValidAuthorityInput](
      _PropertyAuthorityRoundtrip))
    test(PropertyTest[String val](_PropertyInvalidPortRejected))
    test(PropertyTest[String val](_PropertyInvalidHostRejected))
    test(_TestParseURIAuthorityKnownGood)

    // Path segment tests
    test(PropertyTest[String val](_PropertyPathSegmentCount))
    test(PropertyTest[String val](_PropertyPathSegmentRoundtrip))
    test(PropertyTest[String val](_PropertyPathSegmentInvalidRejected))
    test(_TestPathSegmentsKnownGood)

    // Form URL-encoded tests
    test(PropertyTest[Array[(String val, String val)] val](
      _PropertyFormURLEncodedRoundtrip))
    test(PropertyTest[String val](_PropertyFormURLEncodedPlusDecodes))
    test(PropertyTest[String val](
      _PropertyFormURLEncodedInvalidRejected))
    test(_TestFormURLEncodedKnownGood)
    test(_TestURIQueryParams)
    test(_TestFormURLEncodedGet)
    test(_TestFormURLEncodedGetAll)
    test(_TestFormURLEncodedContains)
    test(_TestFormURLEncodedSize)

    // RemoveDotSegments tests
    test(PropertyTest[String val](_PropertyDotSegmentsIdempotent))
    test(PropertyTest[String val](_PropertyDotSegmentsNoDots))
    test(PropertyTest[String val](
      _PropertyDotSegmentsPreservesAbsolute))
    test(_TestRemoveDotSegmentsKnownGood)

    // ResolveURI tests
    test(PropertyTest[_ResolveInput](
      _PropertyResolveResultAbsolute))
    test(PropertyTest[_AbsoluteURIInput](
      _PropertyResolveEmptyRef))
    test(PropertyTest[(_AbsoluteURIInput, _AbsoluteURIInput)](
      _PropertyAbsoluteRefIgnoresBase))
    test(PropertyTest[_ValidURIInput](
      _PropertyNonAbsoluteBaseRejected))
    test(PropertyTest[_ResolveInput](_PropertyResolveRoundtrip))
    test(_TestResolveURIRFCNormal)
    test(_TestResolveURIRFCAbnormal)
    test(_TestResolveURIEdgeCases)

    // NormalizeURI tests
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeIdempotent))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeSchemeLowercase))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeHostLowercase))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeNoEncodedUnreserved))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeUppercaseHex))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeNoDotSegments))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeParseRoundtrip))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeNoDefaultPort))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeNoEmptyPathWithAuthority))
    test(PropertyTest[(_NormalizableURIInput, _NormalizableURIInput)](
      _PropertyNormalizeEquivalentConsistent))
    test(PropertyTest[String val](
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
    test(PropertyTest[_ValidURIInput](_PropertyBuildFromRoundtrip))
    test(PropertyTest[_BuildInput](_PropertyBuildParseRoundtrip))
    test(PropertyTest[String val](_PropertyBuildInvalidSchemeFails))

    // IRI tests (RFC 3987)
    test(PropertyTest[String val](_PropertyIRIToURINoNonASCII))
    test(PropertyTest[String val](_PropertyURIToIRINoEncodedUcschar))
    test(PropertyTest[String val](_PropertyIRIToURIIdempotent))
    test(PropertyTest[String val](_PropertyURIToIRIIdempotent))
    test(PropertyTest[String val](_PropertyIRIToURIRoundtrip))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyNormalizeIRIIdempotent))
    test(PropertyTest[_NormalizableURIInput](
      _PropertyIRIEquivalentReflexive))
    test(PropertyTest[String val](
      _PropertyIRIEquivalentCrossForms))
    test(PropertyTest[String val](
      _PropertyIRIPercentEncodePreservesUcschar))
    test(PropertyTest[String val](
      _PropertyIRIPercentEncodeEncodesNonAllowed))
    test(_TestIRICharsBoundary)
    test(_TestIRIToURIKnownGood)
    test(_TestURIToIRIKnownGood)
    test(_TestNormalizeIRIKnownGood)
