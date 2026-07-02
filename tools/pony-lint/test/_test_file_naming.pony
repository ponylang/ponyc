use "files"
use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestFileNamingSingleEntity is UnitTest
  """Single entity determines the expected filename."""
  fun name(): String => "FileNaming: single entity determines filename"

  fun apply(h: TestHelper) =>
    // File named "test.pony" but principal type is "Foo" -> expects "foo.pony"
    let sf = lint.SourceFile("test.pony", "class Foo\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("Foo", ast.TokenIds.tk_class(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    // test.pony != foo.pony
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_true(diags(0)?.message.contains("foo.pony"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestFileNamingMatchingName is UnitTest
  """File named correctly produces no diagnostics."""
  fun name(): String => "FileNaming: matching name is clean"

  fun apply(h: TestHelper) =>
    // File named "foo.pony" with class Foo -> matches
    let sf = lint.SourceFile("foo.pony", "class Foo\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("Foo", ast.TokenIds.tk_class(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestFileNamingTraitPrincipal is UnitTest
  """Trait with subordinate types is the principal type."""
  fun name(): String => "FileNaming: trait is principal with subordinates"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        "runnable.pony", "trait Runnable\nclass Runner\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("Runnable", ast.TokenIds.tk_trait(), 0, 0))
          .> push(("Runner", ast.TokenIds.tk_class(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestFileNamingMultipleEntitiesSkipped is UnitTest
  """Multiple non-trait entities with no shared prefix produce no diagnostic."""
  fun name(): String =>
    "FileNaming: multiple unrelated entities skipped"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        "helpers.pony", "class Foo\nclass Bar\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("Foo", ast.TokenIds.tk_class(), 0, 0))
          .> push(("Bar", ast.TokenIds.tk_class(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestFileNamingTestPonyMain is UnitTest
  """_test.pony with a Main actor is the standard test runner convention."""
  fun name(): String => "FileNaming: _test.pony with Main is clean"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        "_test.pony", "actor Main is TestList\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("Main", ast.TokenIds.tk_actor(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestFileNamingPrivateType is UnitTest
  """Private type preserves leading underscore in filename."""
  fun name(): String => "FileNaming: private type -> _snake_case filename"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        "_my_helper.pony", "primitive _MyHelper\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("_MyHelper", ast.TokenIds.tk_primitive(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestFileNamingPathWithDirectory is UnitTest
  """Directory components are stripped before comparing to the type name."""
  fun name(): String => "FileNaming: path with directory is stripped"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        Path.join("src", "foo.pony"), "class Foo\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("Foo", ast.TokenIds.tk_class(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestFileNamingPathMismatch is UnitTest
  """Diagnostic message uses the basename, not the full path."""
  fun name(): String => "FileNaming: path mismatch shows basename"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        Path.join("src", "bar.pony"), "class Foo\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("Foo", ast.TokenIds.tk_class(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](1, diags.size())
    try
      h.assert_true(diags(0)?.message.contains("bar.pony"))
    else
      h.fail("could not access diagnostic")
    end

class \nodoc\ _TestFileNamingWindowsPath is UnitTest
  """Backslash path separator is recognized on Windows."""
  fun name(): String => "FileNaming: Windows backslash path is stripped"

  fun apply(h: TestHelper) =>
    // Backslash is only a path separator on Windows; this test is a no-op
    // on other platforms where \ is a valid filename character.
    ifdef windows then
      let sf =
        lint.SourceFile(
          "C:\\Users\\dev\\project\\foo.pony", "class Foo\n", ".")
      let entities =
        recover val
          Array[(String val, ast.TokenId, USize, USize)]
            .> push(("Foo", ast.TokenIds.tk_class(), 0, 0))
        end
      let diags = lint.FileNaming.check_module(entities, sf)
      h.assert_eq[USize](0, diags.size())
    end

class \nodoc\ _TestFileNamingTestPonyMainWithDir is UnitTest
  """_test.pony + Main exemption works with directory-prefixed paths."""
  fun name(): String => "FileNaming: _test.pony with Main and dir is clean"

  fun apply(h: TestHelper) =>
    let sf =
      lint.SourceFile(
        Path.join("src", "_test.pony"), "actor Main is TestList\n", ".")
    let entities =
      recover val
        Array[(String val, ast.TokenId, USize, USize)]
          .> push(("Main", ast.TokenIds.tk_actor(), 0, 0))
      end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())
