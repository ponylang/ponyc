use "pony_test"
use lint = ".."

class \nodoc\ _TestPackageNamingClean is UnitTest
  """snake_case directory names produce no diagnostics."""
  fun name(): String => "PackageNaming: snake_case directory is clean"

  fun apply(h: TestHelper) =>
    let diags = lint.PackageNaming.check_package(
      "my_package", "my_package/foo.pony")
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestPackageNamingViolation is UnitTest
  """Non-snake_case directory names produce diagnostics."""
  fun name(): String => "PackageNaming: non-snake_case directory flagged"

  fun apply(h: TestHelper) =>
    let diags = lint.PackageNaming.check_package(
      "MyPackage", "MyPackage/foo.pony")
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_eq[String]("style/package-naming", diags(0)?.rule_id)
      h.assert_true(diags(0)?.message.contains("MyPackage"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestPackageNamingSimpleName is UnitTest
  """Single-word lowercase directory is clean."""
  fun name(): String => "PackageNaming: simple lowercase name is clean"

  fun apply(h: TestHelper) =>
    let diags = lint.PackageNaming.check_package("lint", "lint/main.pony")
    h.assert_eq[USize](0, diags.size())
