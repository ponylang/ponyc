use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestMethodDeclFormatSingleLine is UnitTest
  """Single-line method declaration is skipped (no diagnostics)."""
  fun name(): String => "MethodDeclarationFormat: single-line skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32, y: U32): U32 => x + y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMethodDeclFormatMultiLineFunClean is UnitTest
  """Multiline fun with each param on own line, correct alignment."""
  fun name(): String => "MethodDeclarationFormat: multiline fun clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun find(\n" +
      "    value: U32,\n" +
      "    offset: USize)\n" +
      "    : USize\n" +
      "  =>\n" +
      "    offset\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMethodDeclFormatMultiLineNoReturnClean is UnitTest
  """Multiline method with no return type (just params and '=>')."""
  fun name(): String => "MethodDeclarationFormat: multiline no return clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(\n" +
      "    x: U32,\n" +
      "    y: U32)\n" +
      "  =>\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMethodDeclFormatNewClean is UnitTest
  """Multiline new constructor with correct formatting."""
  fun name(): String => "MethodDeclarationFormat: new constructor clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  let _x: U32\n" +
      "  let _y: U32\n" +
      "\n" +
      "  new create(\n" +
      "    x: U32,\n" +
      "    y: U32)\n" +
      "  =>\n" +
      "    _x = x\n" +
      "    _y = y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMethodDeclFormatBeClean is UnitTest
  """Multiline be behavior with correct formatting."""
  fun name(): String => "MethodDeclarationFormat: be behavior clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Foo\n" +
      "  be apply(\n" +
      "    x: U32,\n" +
      "    y: U32)\n" +
      "  =>\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](0, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMethodDeclFormatParamsSharingLine is UnitTest
  """Two params sharing a line in multiline declaration produces diagnostic."""
  fun name(): String => "MethodDeclFormat: params sharing line"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(\n" +
      "    x: U32, y: U32,\n" +
      "    z: U32)\n" +
      "  =>\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for params sharing a line")
          try
            h.assert_true(
              diags(0)?.message.contains(
                "each parameter should be on its own line"),
              "message should mention parameter on own line")
          else
            h.fail("could not access diagnostic")
          end
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMethodDeclFormatColonMisaligned is UnitTest
  """':' at wrong column on its own line produces diagnostic."""
  fun name(): String => "MethodDeclFormat: ':' misaligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun find(\n" +
      "    value: U32,\n" +
      "    offset: USize)\n" +
      "      : USize\n" +
      "  =>\n" +
      "    offset\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for ':' misalignment")
          try
            h.assert_true(
              diags(0)?.message.contains("':' should align at column"),
              "message should mention ':' alignment")
          else
            h.fail("could not access diagnostic")
          end
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMethodDeclFormatArrowMisaligned is UnitTest
  """'=>' at wrong column on its own line produces diagnostic."""
  fun name(): String => "MethodDeclFormat: '=>' misaligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun find(\n" +
      "    value: U32,\n" +
      "    offset: USize)\n" +
      "    : USize\n" +
      "    =>\n" +
      "    offset\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match \exhaustive\ program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for '=>' misalignment")
          try
            h.assert_true(
              diags(0)?.message.contains(
                "'=>' should align with 'fun' keyword"),
              "message should mention '=>' alignment")
          else
            h.fail("could not access diagnostic")
          end
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestMethodDeclFormatMultipleViolations is UnitTest
  """Both ':' and '=>' misaligned produces two diagnostics."""
  fun name(): String => "MethodDeclFormat: multiple violations"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun find(\n" +
      "    value: U32,\n" +
      "    offset: USize)\n" +
      "      : USize\n" +
      "    =>\n" +
      "    offset\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.MethodDeclarationFormat)
          h.assert_eq[USize](
            2,
            diags.size(),
            "expected 2 diagnostics for ':' and '=>' misalignment")
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end
