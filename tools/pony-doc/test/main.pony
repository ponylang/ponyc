use "pony_test"
use doc = ".."

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // TQFN tests
    test(_TestTQFNSimple)
    test(_TestTQFNPathSeparator)
    test(_TestTQFNIndexOverride)
    test(_TestTQFNEmptyPackage)

    // NameSort tests
    test(_TestNameSortBasic)
    test(_TestNameSortUnderscore)
    test(_TestNameSortEqual)

    // Filter tests
    test(_TestFilterTestPackage)
    test(_TestFilterPrivate)
    test(_TestFilterInternal)

    // PathSanitize tests
    test(_TestPathSanitizeDot)
    test(_TestPathSanitizeSlash)
    test(_TestPathSanitizeMixed)

    // EntityKind tests
    test(_TestEntityKindStrings)
    test(_TestMethodKindStrings)
    test(_TestFieldKindStrings)

    // TypeRenderer tests
    test(_TestRenderNominalNoLink)
    test(_TestRenderNominalWithLink)
    test(_TestRenderNominalWithTypeArgs)
    test(_TestRenderNominalWithCap)
    test(_TestRenderNominalPrivateNoInclude)
    test(_TestRenderNominalAnonymous)
    test(_TestRenderUnion)
    test(_TestRenderIntersection)
    test(_TestRenderTuple)
    test(_TestRenderArrow)
    test(_TestRenderThis)
    test(_TestRenderCapability)
    test(_TestRenderTypeParamRef)
    test(_TestRenderTypeParamRefEphemeral)
    test(_TestRenderTypeParams)
    test(_TestRenderTypeParamsOptional)
    test(_TestRenderTypeParamsNoConstraint)
    test(_TestRenderTypeParamsEmpty)
    test(_TestRenderNominalCapEphemeral)
    test(_TestRenderBreakLines)
    test(_TestRenderProvides)
