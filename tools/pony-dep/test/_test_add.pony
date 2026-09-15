use "files"
use "pony_test"

use dep = ".."

actor \nodoc\ _TestAddNotify is dep.AddNotify
  let _h: TestHelper
  let _expect_failed: Bool
  let _expected_message: (String val | None)

  new create(
    h: TestHelper,
    expect_failed: Bool = false,
    expected_message: (String val | None) = None)
  =>
    _h = h
    _expect_failed = expect_failed
    _expected_message = expected_message

  be add_failed(message: String val) =>
    if not _expect_failed then
      _h.fail("unexpected add_failed: " + message)
    end
    match _expected_message
    | let sub: String val =>
      _h.assert_true(
        message.contains(sub),
        "expected message containing '" + sub +
          "' but got '" + message + "'")
    end
    _h.complete(true)

  be add_succeeded() =>
    if _expect_failed then
      _h.fail("expected add_failed but got add_succeeded")
    end
    _h.complete(true)

class \nodoc\ _TestAddUnsupportedType is UnitTest
  fun name(): String => "Add rejects unsupported dep type"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let config_path = Path.join(tmp.path.path, "pony.deps")
    let auth = FileAuth(h.env.root)
    dep.Add(
      h.env,
      dep.ConfigAccess(auth, config_path),
      _TestAddNotify(
        h where expect_failed = true,
        expected_message = "unsupported dep type"),
      "foo",
      "https://example.com/foo.par"
      where dep_type = "git")

class \nodoc\ _TestAddDuplicateName is UnitTest
  fun name(): String => "Add rejects duplicate dep name"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let config_path = _ConfigTestHelper.write_config(tmp.path,
      "version 1\n" +
      "\n" +
      "dep foo\n" +
      "  type par\n" +
      "  url https://example.com/foo.par\n" +
      "  hash skip\n" +
      "end\n")?
    let auth = FileAuth(h.env.root)
    dep.Add(
      h.env,
      dep.ConfigAccess(auth, config_path),
      _TestAddNotify(
        h where expect_failed = true,
        expected_message = "already exists"),
      "foo",
      "https://example.com/foo2.par")

class \nodoc\ _TestAddInvalidName is UnitTest
  fun name(): String => "Add rejects invalid dep name"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let config_path = Path.join(tmp.path.path, "pony.deps")
    let auth = FileAuth(h.env.root)
    dep.Add(
      h.env,
      dep.ConfigAccess(auth, config_path),
      _TestAddNotify(
        h where expect_failed = true,
        expected_message = "invalid character"),
      "foo/bar",
      "https://example.com/foo.par")

class \nodoc\ _TestAddConfigParseError is UnitTest
  fun name(): String => "Add fails on config parse error"

  fun apply(h: TestHelper) ? =>
    h.long_test(5_000_000_000)
    let tmp = _TestHelper.tmp_dir(h)?
    let config_path =
      _ConfigTestHelper.write_config(tmp.path, "garbage content\n")?
    let auth = FileAuth(h.env.root)
    dep.Add(
      h.env,
      dep.ConfigAccess(auth, config_path),
      _TestAddNotify(
        h where expect_failed = true,
        expected_message = "expected 'version"),
      "foo",
      "https://example.com/foo.par")
