use "pony_test"
use dep = ".."

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // PathValidator tests
    test(_TestPathValidatorSimplePath)
    test(_TestPathValidatorNestedPath)
    test(_TestPathValidatorEmptyPath)
    test(_TestPathValidatorAbsolutePath)
    test(_TestPathValidatorDotDot)
    test(_TestPathValidatorDot)
    test(_TestPathValidatorBackslash)
    test(_TestPathValidatorNulByte)
    test(_TestPathValidatorConsecutiveSlashes)
    test(_TestPathValidatorTrailingSlash)
    test(_TestPathValidatorDriveLetter)
    test(_TestPathValidatorDotDotSubstring)

    // ArchiveEncoder tests
    test(_TestArchiveEncoderSingleFile)
    test(_TestArchiveEncoderDirectory)
    test(_TestArchiveEncoderNestedDirectories)
    test(_TestArchiveEncoderEmptyDirectory)
    test(_TestArchiveEncoderSkipsSymlinks)
    test(_TestArchiveEncoderDeterministicOrder)
    test(_TestArchiveEncoderRootDirectory)
    test(_TestArchiveEncoderUnreadableFile)

    // ArchiveDecoder tests
    test(_TestArchiveDecoderRoundTrip)
    test(_TestArchiveDecoderRoundTripNested)
    test(_TestArchiveDecoderEmptyDirectory)
    test(_TestArchiveDecoderRoundTripEmptyFile)
    test(_TestArchiveDecoderRejectsTruncatedArchive)
    test(_TestArchiveDecoderErrorsOnMkdirFailure)
    test(_TestArchiveDecoderErrorsOnMissingArchive)
    test(_TestArchiveDecoderRejectsPathTraversal)
    test(_TestArchiveDecoderRejectsUnknownVersion)
    test(_TestArchiveDecoderRejectsUnknownType)
    test(_TestArchiveDecoderRejectsAbsolutePath)
