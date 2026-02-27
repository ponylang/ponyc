use "pony_test"
use lint = ".."

class \nodoc\ _TestDiagnosticString is UnitTest
  fun name(): String => "Diagnostic.string() format"

  fun apply(h: TestHelper) =>
    let d =
      lint.Diagnostic("style/line-length",
        "line exceeds 80 columns (95)", "src/main.pony", 10, 81)
    let expected: String val =
      "src/main.pony:10:81: error[style/line-length]: "
        + "line exceeds 80 columns (95)"
    h.assert_eq[String](expected, d.string())

class \nodoc\ _TestDiagnosticStringSpecialChars is UnitTest
  fun name(): String => "Diagnostic.string() with path containing special chars"

  fun apply(h: TestHelper) =>
    let d =
      lint.Diagnostic("style/hard-tabs", "use spaces",
        "path with spaces/file.pony", 1, 1)
    h.assert_eq[String](
      "path with spaces/file.pony:1:1: error[style/hard-tabs]: use spaces",
      d.string())

class \nodoc\ _TestDiagnosticSortByFile is UnitTest
  fun name(): String => "Diagnostic sorts by file first"

  fun apply(h: TestHelper) =>
    let a = lint.Diagnostic("r", "m", "a.pony", 100, 1)
    let b = lint.Diagnostic("r", "m", "b.pony", 1, 1)
    h.assert_true(a < b)
    h.assert_false(b < a)

class \nodoc\ _TestDiagnosticSortByLine is UnitTest
  fun name(): String => "Diagnostic sorts by line within same file"

  fun apply(h: TestHelper) =>
    let a = lint.Diagnostic("r", "m", "f.pony", 5, 80)
    let b = lint.Diagnostic("r", "m", "f.pony", 10, 1)
    h.assert_true(a < b)
    h.assert_false(b < a)

class \nodoc\ _TestDiagnosticSortByColumn is UnitTest
  fun name(): String => "Diagnostic sorts by column within same file and line"

  fun apply(h: TestHelper) =>
    let a = lint.Diagnostic("r", "m", "f.pony", 5, 3)
    let b = lint.Diagnostic("r", "m", "f.pony", 5, 10)
    h.assert_true(a < b)
    h.assert_false(b < a)

class \nodoc\ _TestDiagnosticEqual is UnitTest
  fun name(): String => "Diagnostic equality"

  fun apply(h: TestHelper) =>
    let a = lint.Diagnostic("r", "m", "f.pony", 5, 3)
    let b = lint.Diagnostic("r", "m", "f.pony", 5, 3)
    h.assert_true(a == b)
    h.assert_false(a < b)
    h.assert_false(b < a)

class \nodoc\ _TestDiagnosticNotEqualDifferentRule is UnitTest
  fun name(): String => "Diagnostics with different rule_id are not equal"

  fun apply(h: TestHelper) =>
    let a = lint.Diagnostic("style/a", "m", "f.pony", 5, 3)
    let b = lint.Diagnostic("style/b", "m", "f.pony", 5, 3)
    h.assert_false(a == b)

class \nodoc\ _TestExitCodes is UnitTest
  fun name(): String => "Exit code values"

  fun apply(h: TestHelper) =>
    h.assert_eq[I32](0, lint.ExitSuccess())
    h.assert_eq[I32](1, lint.ExitViolations())
    h.assert_eq[I32](2, lint.ExitError())
