use "files"
use "pony_test"

use dep = ".."

actor \nodoc\ _TestResolveNotify is dep.ResolveNotify
  let _h: TestHelper
  let _expect_failed: Bool
  let _expected_message: (String val | None)
  let _expected_errors: USize
  let _expected_fetched: USize
  let _expected_skipped: USize
  let _expected_error_substring: (String val | None)

  new create(
    h: TestHelper,
    expect_failed: Bool = false,
    expected_message: (String val | None) = None,
    expected_errors: USize = 0,
    expected_fetched: USize = 0,
    expected_skipped: USize = 0,
    expected_error_substring: (String val | None) = None)
  =>
    _h = h
    _expect_failed = expect_failed
    _expected_message = expected_message
    _expected_errors = expected_errors
    _expected_fetched = expected_fetched
    _expected_skipped = expected_skipped
    _expected_error_substring = expected_error_substring

  be resolve_failed(message: String val) =>
    if not _expect_failed then
      _h.fail("unexpected resolve_failed: " + message)
    end
    match _expected_message
    | let sub: String val =>
      _h.assert_true(
        message.contains(sub),
        "expected message containing '" + sub +
          "' but got '" + message + "'")
    end
    _h.complete(true)

  be resolve_complete(
    errors: Array[(String val, String val)] val,
    fetched: USize,
    skipped: USize)
  =>
    if _expect_failed then
      _h.fail("expected resolve_failed but got resolve_complete")
    else
      _h.assert_eq[USize](_expected_errors, errors.size())
      _h.assert_eq[USize](_expected_fetched, fetched)
      _h.assert_eq[USize](_expected_skipped, skipped)
      match _expected_error_substring
      | let sub: String val =>
        var found = false
        for (_, msg) in errors.values() do
          if msg.contains(sub) then found = true; break end
        end
        _h.assert_true(found,
          "no error message containing '" + sub + "'")
      end
    end
    _h.complete(true)

primitive \nodoc\ _ConfigTestHelper
  fun write_config(dir: FilePath, content: String): String ? =>
    let config_path = dir.join("pony.deps")?
    let f = CreateFile(config_path) as File
    f.print(content)
    f.dispose()
    config_path.path
