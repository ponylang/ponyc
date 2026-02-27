use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestFileNamingSingleEntity is UnitTest
  """Single entity determines the expected filename."""
  fun name(): String => "FileNaming: single entity determines filename"

  fun apply(h: TestHelper) =>
    // File named "test.pony" but principal type is "Foo" -> expects "foo.pony"
    let sf = lint.SourceFile("test.pony", "class Foo\n", ".")
    let entities = recover val
      let a = Array[(String val, ast.TokenId, USize, USize)]
      a.push(("Foo", ast.TokenIds.tk_class(), 0, 0))
      a
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
    let entities = recover val
      let a = Array[(String val, ast.TokenId, USize, USize)]
      a.push(("Foo", ast.TokenIds.tk_class(), 0, 0))
      a
    end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestFileNamingTraitPrincipal is UnitTest
  """Trait with subordinate types is the principal type."""
  fun name(): String => "FileNaming: trait is principal with subordinates"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile(
      "runnable.pony", "trait Runnable\nclass Runner\n", ".")
    let entities = recover val
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
    let sf = lint.SourceFile(
      "helpers.pony", "class Foo\nclass Bar\n", ".")
    let entities = recover val
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
    let sf = lint.SourceFile(
      "_test.pony", "actor Main is TestList\n", ".")
    let entities = recover val
      let a = Array[(String val, ast.TokenId, USize, USize)]
      a.push(("Main", ast.TokenIds.tk_actor(), 0, 0))
      a
    end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())

class \nodoc\ _TestFileNamingPrivateType is UnitTest
  """Private type preserves leading underscore in filename."""
  fun name(): String => "FileNaming: private type -> _snake_case filename"

  fun apply(h: TestHelper) =>
    let sf = lint.SourceFile(
      "_my_helper.pony", "primitive _MyHelper\n", ".")
    let entities = recover val
      let a = Array[(String val, ast.TokenId, USize, USize)]
      a.push(("_MyHelper", ast.TokenIds.tk_primitive(), 0, 0))
      a
    end
    let diags = lint.FileNaming.check_module(entities, sf)
    h.assert_eq[USize](0, diags.size())
