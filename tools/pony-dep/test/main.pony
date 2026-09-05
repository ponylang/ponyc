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

    // Sha256 tests
    test(_TestSha256Empty)
    test(_TestSha256Abc)
    test(_TestSha256TwoBlocks)
    test(_TestSha256OneBlock)
    test(_TestSha256LongMessage)
    test(_TestSha256Hex)

    // ContentHash tests
    test(_TestContentHashSingleFile)
    test(_TestContentHashTwoFiles)
    test(_TestContentHashThreeFiles)
    test(_TestContentHashFourFiles)
    test(_TestContentHashFiveFiles)
    test(_TestContentHashPathBinding)
    test(_TestContentHashEmpty)
    test(_TestContentHashEmptyContent)
    test(_TestContentHashNestedDirs)
    test(_TestContentHashSkipsSymlinks)

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
