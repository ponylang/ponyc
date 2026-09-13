use "files"
use "pony_test"

use dep = ".."

actor \nodoc\ _TestFetchAllNotify is dep.FetchAllNotify
  let _h: TestHelper
  let _expect_failed: Bool
  let _expected_message: (String val | None)
  let _expected_errors: USize
  let _expected_fetched: USize
  let _expected_skipped: USize

  new create(
    h: TestHelper,
    expect_failed: Bool = false,
    expected_message: (String val | None) = None,
    expected_errors: USize = 0,
    expected_fetched: USize = 0,
    expected_skipped: USize = 0)
  =>
    _h = h
    _expect_failed = expect_failed
    _expected_message = expected_message
    _expected_errors = expected_errors
    _expected_fetched = expected_fetched
    _expected_skipped = expected_skipped

  be fetch_all_failed(message: String val) =>
    if not _expect_failed then
      _h.fail("unexpected fetch_all_failed: " + message)
    end
    match _expected_message
    | let sub: String val =>
      _h.assert_true(
        message.contains(sub),
        "expected message containing '" + sub +
          "' but got '" + message + "'")
    end
    _h.complete(true)

  be fetch_all_complete(
    errors: Array[(String val, String val)] val,
    fetched: USize,
    skipped: USize)
  =>
    if _expect_failed then
      _h.fail("expected fetch_all_failed but got fetch_all_complete")
    else
      _h.assert_eq[USize](_expected_errors, errors.size())
      _h.assert_eq[USize](_expected_fetched, fetched)
      _h.assert_eq[USize](_expected_skipped, skipped)
    end
    _h.complete(true)

primitive \nodoc\ _FetchAllTestHelper
  fun write_config(dir: FilePath, content: String): String ? =>
    let config_path = dir.join("pony.deps")?
    let f = CreateFile(config_path) as File
    f.print(content)
    f.dispose()
    config_path.path

class \nodoc\ _TestFetchAllBadDepType is UnitTest
  fun name(): String => "FetchAll rejects unsupported dep type"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let config_path = _FetchAllTestHelper.write_config(tmp.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type git\n" +
      "  url https://example.com/foo\n" +
      "  ref v1.0.0\n" +
      "  hash skip\n" +
      "end\n")?
    let out_dir = Path.join(tmp.path.path, "out")
    dep.FetchAll(
      h.env,
      _TestFetchAllNotify(
        h where expect_failed = true, expected_message = "unsupported dep type"),
      config_path,
      out_dir)

class \nodoc\ _TestFetchAllConfigParseError is UnitTest
  fun name(): String => "FetchAll fails on config parse error"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let config_path =
      _FetchAllTestHelper.write_config(tmp.path, "garbage content\n")?
    let out_dir = Path.join(tmp.path.path, "out")
    dep.FetchAll(
      h.env,
      _TestFetchAllNotify(
        h where expect_failed = true, expected_message = "expected 'version"),
      config_path,
      out_dir)

class \nodoc\ _TestFetchAllConfigNotFound is UnitTest
  fun name(): String => "FetchAll fails when config file not found"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let out_dir = Path.join(tmp.path.path, "out")
    dep.FetchAll(
      h.env,
      _TestFetchAllNotify(
        h where expect_failed = true, expected_message = "cannot read config"),
      Path.join(tmp.path.path, "nonexistent.deps"),
      out_dir)

class \nodoc\ _TestFetchAllSkipsPresent is UnitTest
  fun name(): String => "FetchAll skips deps with existing directories"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?

    let hash_a =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    let hash_b =
      "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb"

    let config_path = _FetchAllTestHelper.write_config(tmp.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  ref v1.0.0\n" +
      "  hash sha256:" + hash_a + "\n" +
      "end\n" +
      "\n" +
      "dep bar\n" +
      "  type par\n" +
      "  url https://example.com/bar.par\n" +
      "  ref v2.0.0\n" +
      "  hash sha256:" + hash_b + "\n" +
      "end\n")?

    let out_dir = Path.join(tmp.path.path, "out")
    let out_path = FilePath(FileAuth(h.env.root), out_dir)
    out_path.mkdir()

    FilePath(FileAuth(h.env.root),
      Path.join(out_dir, "foo@" + hash_a)).mkdir()
    FilePath(FileAuth(h.env.root),
      Path.join(out_dir, "bar@" + hash_b)).mkdir()

    dep.FetchAll(
      h.env,
      _TestFetchAllNotify(h where expected_skipped = 2),
      config_path,
      out_dir)

class \nodoc\ _TestFetchAllSkipsPresentUppercaseHex is UnitTest
  fun name(): String =>
    "FetchAll matches existing dirs with uppercase config hex"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?

    let hash_upper =
      "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855"
    let hash_lower =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

    let config_path = _FetchAllTestHelper.write_config(tmp.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  ref v1.0.0\n" +
      "  hash sha256:" + hash_upper + "\n" +
      "end\n")?

    let out_dir = Path.join(tmp.path.path, "out")
    let out_path = FilePath(FileAuth(h.env.root), out_dir)
    out_path.mkdir()

    FilePath(FileAuth(h.env.root),
      Path.join(out_dir, "foo@" + hash_lower)).mkdir()

    dep.FetchAll(
      h.env,
      _TestFetchAllNotify(h where expected_skipped = 1),
      config_path,
      out_dir)

class \nodoc\ _TestFetchAllEmptyDeps is UnitTest
  fun name(): String => "FetchAll completes with empty deps list"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let config_path =
      _FetchAllTestHelper.write_config(tmp.path, "version 1\n")?
    let out_dir = Path.join(tmp.path.path, "out")
    dep.FetchAll(
      h.env,
      _TestFetchAllNotify(h),
      config_path,
      out_dir)

class \nodoc\ _TestFetchAllOutputNotDirectory is UnitTest
  fun name(): String => "FetchAll fails when output path is a file"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let config_path =
      _FetchAllTestHelper.write_config(tmp.path,
        "version 1\n" +
        "\n" +
        "dep foo\n" +
        "  type par\n" +
        "  url https://example.com/foo.par\n" +
        "  ref v1.0.0\n" +
        "  hash skip\n" +
        "end\n")?

    let out_dir = Path.join(tmp.path.path, "out")
    let out_file = FilePath(FileAuth(h.env.root), out_dir)
    let f = CreateFile(out_file) as File
    f.print("not a directory")
    f.dispose()

    dep.FetchAll(
      h.env,
      _TestFetchAllNotify(
        h where expect_failed = true,
        expected_message = "not a directory"),
      config_path,
      out_dir)
