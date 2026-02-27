use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestDocstringFormatEntityClean is UnitTest
  """Entity with properly formatted multi-line docstring produces 0."""
  fun name(): String => "DocstringFormat: clean entity docstring"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"\n" +
      "  Foo docstring.\n" +
      "  \"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DocstringFormat)
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

class \nodoc\ _TestDocstringFormatMethodClean is UnitTest
  """Method with properly formatted docstring produces 0."""
  fun name(): String => "DocstringFormat: clean method docstring"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"\n" +
      "  Foo docstring.\n" +
      "  \"\"\"\n" +
      "  fun apply(): None =>\n" +
      "    \"\"\"\n" +
      "    Method docstring.\n" +
      "    \"\"\"\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DocstringFormat)
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

class \nodoc\ _TestDocstringFormatSingleLine is UnitTest
  """Single-line entity docstring produces 1 diagnostic."""
  fun name(): String => "DocstringFormat: single-line docstring flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DocstringFormat)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/docstring-format", diags(0)?.rule_id)
            h.assert_true(
              diags(0)?.message.contains("opening"))
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

class \nodoc\ _TestDocstringFormatClosingContent is UnitTest
  """Closing \"\"\" with content on same line produces 1 diagnostic."""
  fun name(): String => "DocstringFormat: content on closing line flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"\n" +
      "  Foo docstring.\n" +
      "  closing text.\"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DocstringFormat)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_true(
              diags(0)?.message.contains("closing"))
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

class \nodoc\ _TestDocstringFormatNoDocstring is UnitTest
  """Entity without docstring produces 0 (nothing to format-check)."""
  fun name(): String => "DocstringFormat: no docstring is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val = "class Foo\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DocstringFormat)
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

class \nodoc\ _TestDocstringFormatNodocEntityExempt is UnitTest
  """\\nodoc\\ entity with single-line docstring produces 0 (exempt)."""
  fun name(): String => "DocstringFormat: \\nodoc\\ entity exempt"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class \\nodoc\\ Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DocstringFormat)
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

class \nodoc\ _TestDocstringFormatNodocEntityMethodExempt is UnitTest
  """Method inside \\nodoc\\ entity with bad docstring produces 0."""
  fun name(): String =>
    "DocstringFormat: method in \\nodoc\\ entity exempt"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class \\nodoc\\ Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  fun apply(): None =>\n" +
      "    \"\"\"Method docstring.\"\"\"\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.DocstringFormat)
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

