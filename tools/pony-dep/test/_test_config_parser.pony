use "pony_test"
use dep = ".."

class \nodoc\ _TestConfigParserMinimalValid is UnitTest
  fun name(): String => "ConfigParser/minimal valid config"

  fun apply(h: TestHelper) =>
    let input = "version 1\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.assert_eq[U64](c.version, 1)
      h.assert_eq[USize](c.deps.size(), 0)
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserSingleDep is UnitTest
  fun name(): String => "ConfigParser/single dependency"

  fun apply(h: TestHelper) =>
    let hash: String val =
      "sha256:" +
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    let input: String val =
      "version 1\n" +
      "\n" +
      "dep msgpack\n" +
      "  type git\n" +
      "  url https://github.com/ponylang/pony-msgpack.git\n" +
      "  ref 0.3.0\n" +
      "  hash " + hash + "\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.assert_eq[U64](c.version, 1)
      h.assert_eq[USize](c.deps.size(), 1)
      try
        let d = c.deps(0)?
        h.assert_eq[String](d.name, "msgpack")
        h.assert_eq[String](d.dep_type, "git")
        h.assert_eq[String](d.url,
          "https://github.com/ponylang/pony-msgpack.git")
        h.assert_eq[String](d.ref_name, "0.3.0")
        h.assert_eq[String](d.hash, hash)
        match d.documentation_url
        | let s: String => h.fail("expected None, got '" + s + "'")
        end
      else
        h.fail("deps(0) out of bounds")
      end
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserDocUrl is UnitTest
  fun name(): String => "ConfigParser/optional documentation_url"

  fun apply(h: TestHelper) =>
    let hash: String val =
      "sha256:" +
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    let input: String val =
      "version 1\n" +
      "dep msgpack\n" +
      "  type git\n" +
      "  url https://github.com/ponylang/pony-msgpack.git\n" +
      "  ref 0.3.0\n" +
      "  hash " + hash + "\n" +
      "  documentation_url https://ponylang.github.io/pony-msgpack/\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      try
        let d = c.deps(0)?
        match d.documentation_url
        | let s: String =>
          h.assert_eq[String](s,
            "https://ponylang.github.io/pony-msgpack/")
        | None =>
          h.fail("expected documentation_url, got None")
        end
      else
        h.fail("deps(0) out of bounds")
      end
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserMultipleDeps is UnitTest
  fun name(): String => "ConfigParser/multiple dependencies"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "\n" +
      "dep msgpack\n" +
      "  type git\n" +
      "  url https://github.com/ponylang/pony-msgpack.git\n" +
      "  ref 0.3.0\n" +
      "  hash skip\n" +
      "end\n" +
      "\n" +
      "dep http\n" +
      "  type git\n" +
      "  url https://github.com/ponylang/pony-http.git\n" +
      "  ref 0.6.0\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.assert_eq[USize](c.deps.size(), 2)
      try
        h.assert_eq[String](c.deps(0)?.name, "msgpack")
        h.assert_eq[String](c.deps(1)?.name, "http")
        h.assert_eq[String](c.deps(1)?.url,
          "https://github.com/ponylang/pony-http.git")
        h.assert_eq[String](c.deps(1)?.ref_name, "0.6.0")
      else
        h.fail("deps out of bounds")
      end
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserVersionTab is UnitTest
  fun name(): String => "ConfigParser/version with tab separator"

  fun apply(h: TestHelper) =>
    let input = "version\t1\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.assert_eq[U64](c.version, 1)
      h.assert_eq[USize](c.deps.size(), 0)
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserComments is UnitTest
  fun name(): String => "ConfigParser/comments and blank lines"

  fun apply(h: TestHelper) =>
    let input: String val =
      "# pony.deps\n" +
      "\n" +
      "version 1\n" +
      "\n" +
      "# message pack codec\n" +
      "dep msgpack\n" +
      "  type git\n" +
      "  # pinned to latest stable\n" +
      "  url https://github.com/ponylang/pony-msgpack.git\n" +
      "  ref 0.3.0\n" +
      "  hash skip\n" +
      "end\n" +
      "\n" +
      "# end of file\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.assert_eq[USize](c.deps.size(), 1)
      try
        h.assert_eq[String](c.deps(0)?.name, "msgpack")
      else
        h.fail("deps(0) out of bounds")
      end
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserHashSkip is UnitTest
  fun name(): String => "ConfigParser/hash skip"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep sketchy\n" +
      "  type http\n" +
      "  url https://example.com/sketchy.par\n" +
      "  ref 0.1.0\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      try
        h.assert_eq[String](c.deps(0)?.hash, "skip")
      else
        h.fail("deps(0) out of bounds")
      end
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserUnknownFields is UnitTest
  fun name(): String => "ConfigParser/unknown fields skipped"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep msgpack\n" +
      "  type git\n" +
      "  url https://github.com/ponylang/pony-msgpack.git\n" +
      "  ref 0.3.0\n" +
      "  hash skip\n" +
      "  future_field some_value\n" +
      "  another_field 42\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.assert_eq[USize](c.deps.size(), 1)
      try
        h.assert_eq[String](c.deps(0)?.name, "msgpack")
      else
        h.fail("deps(0) out of bounds")
      end
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserNoTrailingNewline is UnitTest
  fun name(): String => "ConfigParser/no trailing newline"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url https://example.com/foo.git\n" +
      "  ref main\n" +
      "  hash skip\n" +
      "end"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.assert_eq[USize](c.deps.size(), 1)
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

// Error cases

class \nodoc\ _TestConfigParserEmptyFile is UnitTest
  fun name(): String => "ConfigParser/error: empty file"

  fun apply(h: TestHelper) =>
    match dep.ConfigParser("")
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 0)
    end

class \nodoc\ _TestConfigParserMissingVersion is UnitTest
  fun name(): String => "ConfigParser/error: missing version"

  fun apply(h: TestHelper) =>
    let input = "# just a comment\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 0)
    end

class \nodoc\ _TestConfigParserContentBeforeVersion is UnitTest
  fun name(): String => "ConfigParser/error: content before version"

  fun apply(h: TestHelper) =>
    let input: String val =
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n" +
      "version 1\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 1)
    end

class \nodoc\ _TestConfigParserUnsupportedVersion is UnitTest
  fun name(): String => "ConfigParser/error: unsupported version"

  fun apply(h: TestHelper) =>
    match dep.ConfigParser("version 2\n")
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 1)
    end

class \nodoc\ _TestConfigParserVersionZero is UnitTest
  fun name(): String => "ConfigParser/error: version 0"

  fun apply(h: TestHelper) =>
    match dep.ConfigParser("version 0\n")
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 1)
    end

class \nodoc\ _TestConfigParserInvalidVersionNumber is UnitTest
  fun name(): String => "ConfigParser/error: invalid version number"

  fun apply(h: TestHelper) =>
    match dep.ConfigParser("version abc\n")
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 1)
    end

class \nodoc\ _TestConfigParserMissingEnd is UnitTest
  fun name(): String => "ConfigParser/error: missing end"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
    end

class \nodoc\ _TestConfigParserMissingRequiredField is UnitTest
  fun name(): String => "ConfigParser/error: missing required field"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 6)
      h.assert_true(e.message.contains("'hash'"))
    end

class \nodoc\ _TestConfigParserMissingType is UnitTest
  fun name(): String => "ConfigParser/error: missing type field"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 6)
      h.assert_true(e.message.contains("'type'"))
    end

class \nodoc\ _TestConfigParserMissingUrl is UnitTest
  fun name(): String => "ConfigParser/error: missing url field"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 6)
      h.assert_true(e.message.contains("'url'"))
    end

class \nodoc\ _TestConfigParserMissingRef is UnitTest
  fun name(): String => "ConfigParser/error: missing ref field"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 6)
      h.assert_true(e.message.contains("'ref'"))
    end

class \nodoc\ _TestConfigParserDuplicateField is UnitTest
  fun name(): String => "ConfigParser/error: duplicate field"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  type http\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 4)
    end

class \nodoc\ _TestConfigParserDuplicateDepName is UnitTest
  fun name(): String => "ConfigParser/error: duplicate dep name"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url z\n" +
      "  ref w\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 8)
    end

class \nodoc\ _TestConfigParserNestedDep is UnitTest
  fun name(): String => "ConfigParser/error: nested dep"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  dep bar\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 4)
    end

class \nodoc\ _TestConfigParserEmptyDepName is UnitTest
  fun name(): String => "ConfigParser/error: empty dep name"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep \n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
    end

class \nodoc\ _TestConfigParserDepNameWithSpaces is UnitTest
  fun name(): String => "ConfigParser/error: dep name with spaces"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo bar\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
    end

class \nodoc\ _TestConfigParserDepNameWithTabs is UnitTest
  fun name(): String => "ConfigParser/error: dep name with tabs"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\tbar\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
    end

class \nodoc\ _TestConfigParserEndTrailingContent is UnitTest
  fun name(): String => "ConfigParser/error: trailing content after end"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end stuff\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 7)
    end

class \nodoc\ _TestConfigParserOrphanEnd is UnitTest
  fun name(): String => "ConfigParser/error: end without dep"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
    end

class \nodoc\ _TestConfigParserUnexpectedTopLevel is UnitTest
  fun name(): String => "ConfigParser/error: unexpected top-level line"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "something unexpected\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
    end

class \nodoc\ _TestConfigParserDuplicateVersion is UnitTest
  fun name(): String => "ConfigParser/error: duplicate version line"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "version 1\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
    end

class \nodoc\ _TestConfigParserInvalidHashFormat is UnitTest
  fun name(): String => "ConfigParser/error: invalid hash format"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash md5:abc123\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 6)
    end

class \nodoc\ _TestConfigParserHashWrongLength is UnitTest
  fun name(): String => "ConfigParser/error: hash wrong length"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash sha256:abc123\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 6)
    end

class \nodoc\ _TestConfigParserHashBadHex is UnitTest
  fun name(): String => "ConfigParser/error: non-hex character in hash"

  fun apply(h: TestHelper) =>
    let bad_hash: String val =
      "sha256:" +
      "g3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash " + bad_hash + "\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 6)
    end

class \nodoc\ _TestConfigParserFieldMissingValue is UnitTest
  fun name(): String => "ConfigParser/error: field without value"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 3)
    end

class \nodoc\ _TestConfigParserDepNoName is UnitTest
  fun name(): String => "ConfigParser/error: dep keyword alone"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
    end

class \nodoc\ _TestConfigParserHashUppercaseHex is UnitTest
  fun name(): String => "ConfigParser/uppercase hex in hash accepted"

  fun apply(h: TestHelper) =>
    let hash: String val =
      "sha256:" +
      "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855"
    let input: String val =
      "version 1\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash " + hash + "\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.assert_eq[USize](c.deps.size(), 1)
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigParserDepNameSlash is UnitTest
  fun name(): String => "ConfigParser/error: dep name with slash"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep ../escape\n" +
      "  type par\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
      h.assert_true(e.message.contains("invalid character"))
    end

class \nodoc\ _TestConfigParserDepNameBackslash is UnitTest
  fun name(): String => "ConfigParser/error: dep name with backslash"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep ..\\escape\n" +
      "  type par\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
      h.assert_true(e.message.contains("invalid character"))
    end

class \nodoc\ _TestConfigParserDepNameDotDot is UnitTest
  fun name(): String => "ConfigParser/error: dep name is '..'"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep ..\n" +
      "  type par\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
      h.assert_true(e.message.contains("'.' or '..'"))
    end

class \nodoc\ _TestConfigParserDepNameDot is UnitTest
  fun name(): String => "ConfigParser/error: dep name is '.'"

  fun apply(h: TestHelper) =>
    let input: String val =
      "version 1\n" +
      "dep .\n" +
      "  type par\n" +
      "  url x\n" +
      "  ref y\n" +
      "  hash skip\n" +
      "end\n"
    match dep.ConfigParser(input)
    | let c: dep.ConfigFile =>
      h.fail("expected error, got ConfigFile")
    | let e: dep.ConfigError =>
      h.assert_eq[USize](e.line, 2)
      h.assert_true(e.message.contains("'.' or '..'"))
    end
