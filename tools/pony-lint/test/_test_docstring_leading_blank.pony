use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestDocLeadingBlankEntityFlagged is UnitTest
  """Entity docstring with blank line after opening \"\"\" is flagged."""
  fun name(): String => "DocLeadingBlank: entity leading blank flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"\n" +
      "\n" +
      "  Foo docstring.\n" +
      "  \"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.DocstringLeadingBlank)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/docstring-leading-blank", diags(0)?.rule_id)
            h.assert_true(
              diags(0)?.message.contains("blank line"))
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

class \nodoc\ _TestDocLeadingBlankEntityClean is UnitTest
  """Entity docstring without leading blank is clean."""
  fun name(): String => "DocLeadingBlank: entity no leading blank clean"
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
          let diags =
            _CollectRuleDiags(mod, sf, lint.DocstringLeadingBlank)
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

class \nodoc\ _TestDocLeadingBlankMethodFlagged is UnitTest
  """Method docstring with blank line after opening \"\"\" is flagged."""
  fun name(): String => "DocLeadingBlank: method leading blank flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  \"\"\"\n" +
      "  Foo docstring.\n" +
      "  \"\"\"\n" +
      "  fun apply(): None =>\n" +
      "    \"\"\"\n" +
      "\n" +
      "    Method docstring.\n" +
      "    \"\"\"\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.DocstringLeadingBlank)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[USize](7, diags(0)?.line)
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

class \nodoc\ _TestDocLeadingBlankMethodClean is UnitTest
  """Method docstring without leading blank is clean."""
  fun name(): String => "DocLeadingBlank: method no leading blank clean"
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
          let diags =
            _CollectRuleDiags(mod, sf, lint.DocstringLeadingBlank)
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

class \nodoc\ _TestDocLeadingBlankAbstractMethodFlagged is UnitTest
  """Abstract method docstring with leading blank is flagged."""
  fun name(): String =>
    "DocLeadingBlank: abstract method leading blank flagged"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "trait Foo\n" +
      "  fun bar(): None\n" +
      "    \"\"\"\n" +
      "\n" +
      "    Bar docs.\n" +
      "    \"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.DocstringLeadingBlank)
          h.assert_eq[USize](1, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestDocLeadingBlankSingleLineClean is UnitTest
  """Single-line docstring does not trigger this rule."""
  fun name(): String => "DocLeadingBlank: single-line docstring clean"
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
          let diags =
            _CollectRuleDiags(mod, sf, lint.DocstringLeadingBlank)
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

class \nodoc\ _TestDocLeadingBlankNodocEntityExempt is UnitTest
  """\nodoc\ entity with leading blank in docstring is exempt."""
  fun name(): String => "DocLeadingBlank: \\nodoc\\ entity exempt"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class \\nodoc\\ Foo\n" +
      "  \"\"\"\n" +
      "\n" +
      "  Foo docstring.\n" +
      "  \"\"\"\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.DocstringLeadingBlank)
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

class \nodoc\ _TestDocLeadingBlankNodocEntityMethodExempt is UnitTest
  """Method inside \nodoc\ entity with leading blank is exempt."""
  fun name(): String =>
    "DocLeadingBlank: method in \\nodoc\\ entity exempt"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class \\nodoc\\ Foo\n" +
      "  \"\"\"Foo docstring.\"\"\"\n" +
      "  fun apply(): None =>\n" +
      "    \"\"\"\n" +
      "\n" +
      "    Method docstring.\n" +
      "    \"\"\"\n" +
      "    None\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.DocstringLeadingBlank)
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
