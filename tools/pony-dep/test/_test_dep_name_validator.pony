use "pony_test"

use dep = ".."

class \nodoc\ _TestDepNameValidatorValid is UnitTest
  fun name(): String => "DepNameValidator/valid names"

  fun apply(h: TestHelper) =>
    let names: Array[String val] =
      ["foo"; "my-lib"; "lib_v2"; "a"]
    for n in names.values() do
      match dep.DepNameValidator(n)
      | let e: String val =>
        h.fail("expected valid name '" + n + "' but got: " + e)
      end
    end

class \nodoc\ _TestDepNameValidatorEmpty is UnitTest
  fun name(): String => "DepNameValidator/empty name"

  fun apply(h: TestHelper) =>
    match dep.DepNameValidator("")
    | let e: String val =>
      h.assert_true(e.contains("requires a name"))
    | None => h.fail("expected error for empty name")
    end

class \nodoc\ _TestDepNameValidatorSlash is UnitTest
  fun name(): String => "DepNameValidator/slash in name"

  fun apply(h: TestHelper) =>
    match dep.DepNameValidator("foo/bar")
    | let e: String val =>
      h.assert_true(e.contains("invalid character"))
    | None => h.fail("expected error for name with slash")
    end

class \nodoc\ _TestDepNameValidatorDotDot is UnitTest
  fun name(): String => "DepNameValidator/dot-dot name"

  fun apply(h: TestHelper) =>
    match dep.DepNameValidator("..")
    | let e: String val =>
      h.assert_true(e.contains("'.' or '..'"))
    | None => h.fail("expected error for '..' name")
    end

class \nodoc\ _TestDepNameValidatorSpace is UnitTest
  fun name(): String => "DepNameValidator/space in name"

  fun apply(h: TestHelper) =>
    match dep.DepNameValidator("foo bar")
    | let e: String val =>
      h.assert_true(e.contains("single token"))
    | None => h.fail("expected error for name with space")
    end
