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
    test.property(_PropertyPercentRoundtrip)
    test.property(_PropertyPercentEncodeOutputLegal)
    test.property(_PropertyInvalidPercentSequenceRejected)
    test.property(_PropertyPercentDecodeBoundary)
    test(_TestPercentEncodeKnownGood)

    // URI parsing tests
    test.property(_PropertyURIRoundtrip)
    test.property(_PropertyInvalidSchemeRejected)
    test(_TestParseURIKnownGood)

    // Authority parsing tests
    test.property(_PropertyAuthorityRoundtrip)
    test.property(_PropertyInvalidPortRejected)
    test.property(_PropertyInvalidHostRejected)
    test(_TestParseURIAuthorityKnownGood)

    // Path segment tests
    test.property(_PropertyPathSegmentCount)
    test.property(_PropertyPathSegmentRoundtrip)
    test.property(_PropertyPathSegmentInvalidRejected)
    test(_TestPathSegmentsKnownGood)

    // Form URL-encoded tests
    test.property(_PropertyFormURLEncodedRoundtrip)
    test.property(_PropertyFormURLEncodedPlusDecodes)
    test.property(_PropertyFormURLEncodedInvalidRejected)
    test(_TestFormURLEncodedKnownGood)
    test(_TestURIQueryParams)
    test(_TestFormURLEncodedGet)
    test(_TestFormURLEncodedGetAll)
    test(_TestFormURLEncodedContains)
    test(_TestFormURLEncodedSize)

    // RemoveDotSegments tests
    test.property(_PropertyDotSegmentsIdempotent)
    test.property(_PropertyDotSegmentsNoDots)
    test.property(_PropertyDotSegmentsPreservesAbsolute)
    test(_TestRemoveDotSegmentsKnownGood)

    // ResolveURI tests
    test.property(_PropertyResolveResultAbsolute)
    test.property(_PropertyResolveEmptyRef)
    test.property(_PropertyAbsoluteRefIgnoresBase)
    test.property(_PropertyNonAbsoluteBaseRejected)
    test.property(_PropertyResolveRoundtrip)
    test(_TestResolveURIRFCNormal)
    test(_TestResolveURIRFCAbnormal)
    test(_TestResolveURIEdgeCases)

    // NormalizeURI tests
    test.property(_PropertyNormalizeIdempotent)
    test.property(_PropertyNormalizeSchemeLowercase)
    test.property(_PropertyNormalizeHostLowercase)
    test.property(_PropertyNormalizeNoEncodedUnreserved)
    test.property(_PropertyNormalizeUppercaseHex)
    test.property(_PropertyNormalizeNoDotSegments)
    test.property(_PropertyNormalizeParseRoundtrip)
    test.property(_PropertyNormalizeNoDefaultPort)
    test.property(_PropertyNormalizeNoEmptyPathWithAuthority)
    test.property(_PropertyNormalizeEquivalentConsistent)
    test.property(_PropertyNormalizeInvalidPercentRejected)
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
    test.property(_PropertyBuildFromRoundtrip)
    test.property(_PropertyBuildParseRoundtrip)
    test.property(_PropertyBuildInvalidSchemeFails)

    // IRI tests (RFC 3987)
    test.property(_PropertyIRIToURINoNonASCII)
    test.property(_PropertyURIToIRINoEncodedUcschar)
    test.property(_PropertyIRIToURIIdempotent)
    test.property(_PropertyURIToIRIIdempotent)
    test.property(_PropertyIRIToURIRoundtrip)
    test.property(_PropertyNormalizeIRIIdempotent)
    test.property(_PropertyIRIEquivalentReflexive)
    test.property(_PropertyIRIEquivalentCrossForms)
    test.property(_PropertyIRIPercentEncodePreservesUcschar)
    test.property(_PropertyIRIPercentEncodeEncodesNonAllowed)
    test(_TestIRICharsBoundary)
    test(_TestIRIToURIKnownGood)
    test(_TestURIToIRIKnownGood)
    test(_TestNormalizeIRIKnownGood)
