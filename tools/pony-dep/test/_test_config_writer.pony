use "pony_test"
use dep = ".."

class \nodoc\ _TestConfigWriterAllFields is UnitTest
  fun name(): String => "ConfigWriter/all fields"

  fun apply(h: TestHelper) =>
    let result = dep.ConfigWriter.format_entry(
      "msgpack", "par",
      "https://example.com/msgpack.par",
      "sha256:abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
      where ref_name = "0.3.0",
        documentation_url = "https://docs.example.com/")
    h.assert_true(result.contains("dep msgpack"))
    h.assert_true(result.contains("type par"))
    h.assert_true(result.contains(
      "url https://example.com/msgpack.par"))
    h.assert_true(result.contains("ref 0.3.0"))
    h.assert_true(result.contains("hash sha256:abcd1234"))
    h.assert_true(result.contains(
      "documentation_url https://docs.example.com/"))
    h.assert_true(result.contains("end"))

class \nodoc\ _TestConfigWriterWithoutRef is UnitTest
  fun name(): String => "ConfigWriter/without ref"

  fun apply(h: TestHelper) =>
    let result = dep.ConfigWriter.format_entry(
      "foo", "par",
      "https://example.com/foo.par",
      "sha256:abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234")
    h.assert_false(result.contains("\n  ref "))
    h.assert_true(result.contains("dep foo"))
    h.assert_true(result.contains("end"))

class \nodoc\ _TestConfigWriterWithoutDocUrl is UnitTest
  fun name(): String => "ConfigWriter/without documentation_url"

  fun apply(h: TestHelper) =>
    let result = dep.ConfigWriter.format_entry(
      "foo", "par",
      "https://example.com/foo.par",
      "sha256:abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"
      where ref_name = "1.0.0")
    h.assert_false(result.contains("documentation_url"))
    h.assert_true(result.contains("ref 1.0.0"))

class \nodoc\ _TestConfigWriterRoundTrip is UnitTest
  fun name(): String => "ConfigWriter/round-trip through parser"

  fun apply(h: TestHelper) =>
    let hash =
      "sha256:" +
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    let entry = dep.ConfigWriter.format_entry(
      "mylib", "par",
      "https://example.com/mylib.par",
      hash
      where ref_name = "2.0.0",
        documentation_url = "https://docs.example.com/mylib/")
    let config = dep.ConfigWriter.new_config_with(entry)

    match dep.ConfigParser(config)
    | let c: dep.ConfigFile =>
      h.assert_eq[U64](c.version, 1)
      h.assert_eq[USize](c.deps.size(), 1)
      try
        let d = c.deps(0)?
        h.assert_eq[String](d.name, "mylib")
        h.assert_eq[String](d.dep_type, "par")
        h.assert_eq[String](d.url,
          "https://example.com/mylib.par")
        match d.ref_name
        | let r: String val => h.assert_eq[String](r, "2.0.0")
        | None => h.fail("expected ref_name '2.0.0' but got None")
        end
        h.assert_eq[String](d.hash, hash)
        match d.documentation_url
        | let du: String val =>
          h.assert_eq[String](du,
            "https://docs.example.com/mylib/")
        | None => h.fail("expected documentation_url but got None")
        end
      else
        h.fail("deps(0) out of bounds")
      end
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end

class \nodoc\ _TestConfigWriterRoundTripNoRef is UnitTest
  fun name(): String => "ConfigWriter/round-trip without ref"

  fun apply(h: TestHelper) =>
    let hash =
      "sha256:" +
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    let entry = dep.ConfigWriter.format_entry(
      "mylib", "par",
      "https://example.com/mylib.par",
      hash)
    let config = dep.ConfigWriter.new_config_with(entry)

    match dep.ConfigParser(config)
    | let c: dep.ConfigFile =>
      h.assert_eq[USize](c.deps.size(), 1)
      try
        let d = c.deps(0)?
        h.assert_eq[String](d.name, "mylib")
        match d.ref_name
        | let _: String val =>
          h.fail("expected None ref_name")
        | None => None
        end
      else
        h.fail("deps(0) out of bounds")
      end
    | let e: dep.ConfigError =>
      h.fail(e.string())
    end
