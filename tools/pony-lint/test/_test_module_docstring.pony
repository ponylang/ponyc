use "pony_test"
use lint = ".."

class \nodoc\ _TestModuleDocstringClean is UnitTest
  """Module with expected file and docstring produces 0."""
  fun name(): String => "ModuleDocstring: has file and docstring"

  fun apply(h: TestHelper) =>
    let diags =
      lint.ModuleDocstring.check_module(
        "my_package", true, true, "my_package/foo.pony")
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestModuleDocstringNoFile is UnitTest
  """Module without expected file produces 1 diagnostic."""
  fun name(): String => "ModuleDocstring: no module file flagged"

  fun apply(h: TestHelper) =>
    let diags =
      lint.ModuleDocstring.check_module(
        "my_package", false, false, "my_package/foo.pony")
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[String](
        "style/module-docstring", diags(0)?.rule_id)
      h.assert_true(
        diags(0)?.message.contains("no 'my_package.pony'"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestModuleDocstringNoDocstring is UnitTest
  """Module with file but no docstring produces 1 diagnostic."""
  fun name(): String => "ModuleDocstring: no docstring flagged"

  fun apply(h: TestHelper) =>
    let diags =
      lint.ModuleDocstring.check_module(
        "my_package", true, false, "my_package/foo.pony")
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[String](
        "style/module-docstring", diags(0)?.rule_id)
      h.assert_true(
        diags(0)?.message.contains("should have a module docstring"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestModuleDocstringHyphenated is UnitTest
  """Hyphenated module name with underscored file produces 0."""
  fun name(): String => "ModuleDocstring: hyphenated name normalized"

  fun apply(h: TestHelper) =>
    // The linter normalizes hyphens to underscores before calling
    // check_module, so we test with the already-normalized name
    let diags =
      lint.ModuleDocstring.check_module(
        "pony_lint", true, true, "pony-lint/foo.pony")
    h.assert_eq[USize](0, diags.size())
