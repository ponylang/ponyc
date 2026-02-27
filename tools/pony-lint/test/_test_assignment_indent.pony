use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestAssignmentIndentSingleLine is UnitTest
  """Single-line assignment produces no diagnostics."""
  fun name(): String => "AssignmentIndent: single-line assignment is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): U32 =>\n" +
      "    let x: U32 = U32(42)\n" +
      "    x\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AssignmentIndent)
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

class \nodoc\ _TestAssignmentIndentRHSNextLine is UnitTest
  """RHS starting on the line after '=' produces no diagnostics."""
  fun name(): String => "AssignmentIndent: RHS on next line is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): U32 =>\n" +
      "    let y =\n" +
      "      if x > 0 then\n" +
      "        x\n" +
      "      else\n" +
      "        U32(0)\n" +
      "      end\n" +
      "    y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AssignmentIndent)
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

class \nodoc\ _TestAssignmentIndentMultiLineNextLine is UnitTest
  """Multiline recover starting on the line after '=' is clean."""
  fun name(): String => "AssignmentIndent: recover on next line is clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): Array[U8] val =>\n" +
      "    let output =\n" +
      "      recover val\n" +
      "        Array[U8]\n" +
      "      end\n" +
      "    output\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AssignmentIndent)
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

class \nodoc\ _TestAssignmentIndentViolation is UnitTest
  """Multiline if starting on the '=' line produces 1 diagnostic."""
  fun name(): String => "AssignmentIndent: multiline if on '=' line flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): U32 =>\n" +
      "    var y: U32 = if x > 0 then\n" +
      "      x\n" +
      "    else\n" +
      "      U32(0)\n" +
      "    end\n" +
      "    y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AssignmentIndent)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/assignment-indent", diags(0)?.rule_id)
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

class \nodoc\ _TestAssignmentIndentRecoverViolation is UnitTest
  """Multiline recover starting on the '=' line produces 1 diagnostic."""
  fun name(): String => "AssignmentIndent: recover on '=' line flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): Array[U8] val =>\n" +
      "    let arr = recover val\n" +
      "      Array[U8]\n" +
      "    end\n" +
      "    arr\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AssignmentIndent)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/assignment-indent", diags(0)?.rule_id)
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

class \nodoc\ _TestAssignmentIndentReassignment is UnitTest
  """Multiline reassignment (no let/var) produces 1 diagnostic."""
  fun name(): String => "AssignmentIndent: reassignment flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): U32 =>\n" +
      "    var y: U32 = 0\n" +
      "    y = if x > 0 then\n" +
      "      x\n" +
      "    else\n" +
      "      U32(0)\n" +
      "    end\n" +
      "    y\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AssignmentIndent)
          h.assert_eq[USize](1, diags.size())
          try
            h.assert_eq[String](
              "style/assignment-indent", diags(0)?.rule_id)
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

class \nodoc\ _TestAssignmentIndentMultipleViolations is UnitTest
  """Two multiline assignments on '=' lines produce 2 diagnostics."""
  fun name(): String => "AssignmentIndent: multiple violations"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(x: U32): U32 =>\n" +
      "    var a: U32 = if x > 0 then\n" +
      "      x\n" +
      "    else\n" +
      "      U32(0)\n" +
      "    end\n" +
      "    let b: U32 = if a > 1 then\n" +
      "      a\n" +
      "    else\n" +
      "      U32(1)\n" +
      "    end\n" +
      "    b\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags = _CollectRuleDiags(mod, sf, lint.AssignmentIndent)
          h.assert_eq[USize](2, diags.size())
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end
