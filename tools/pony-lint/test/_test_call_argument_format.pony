use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestCallArgFmtSingleLine is UnitTest
  """Single-line call with multiple args produces no diagnostics."""
  fun name(): String => "CallArgumentFormat: single-line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1), U32(2))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
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

class \nodoc\ _TestCallArgFmtAllOnNextLine is UnitTest
  """All args on one continuation line produces no diagnostics."""
  fun name(): String => "CallArgumentFormat: all on next line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(\n" +
      "      U32(1), U32(2), U32(3))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
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

class \nodoc\ _TestCallArgFmtEachOnOwnLine is UnitTest
  """Each arg on its own line produces no diagnostics."""
  fun name(): String => "CallArgumentFormat: each on own line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(\n" +
      "      U32(1),\n" +
      "      U32(2),\n" +
      "      U32(3))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
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

class \nodoc\ _TestCallArgFmtWhereClauseOwnLine is UnitTest
  """Where clause on own line with positional args produces no diagnostics."""
  fun name(): String => "CallArgumentFormat: where on own line clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun bar(x: U32, y: U32 = 0) => None\n" +
      "\n" +
      "  fun apply(): None =>\n" +
      "    bar(\n" +
      "      U32(1)\n" +
      "      where y = U32(2))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
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

class \nodoc\ _TestCallArgFmtSingleArgMultiline is UnitTest
  """Single arg on next line (multiline due to arg complexity) is skipped."""
  fun name(): String => "CallArgumentFormat: single arg multiline skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(\n" +
      "      U32(1))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
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

class \nodoc\ _TestCallArgFmtNoArgs is UnitTest
  """Call with no args produces no diagnostics."""
  fun name(): String => "CallArgumentFormat: no args clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz()\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
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

class \nodoc\ _TestCallArgFmtPartialSplit is UnitTest
  """First arg on '(' line, rest on continuation — flagged."""
  fun name(): String => "CallArgumentFormat: partial split flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1),\n" +
      "      U32(2), U32(3))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
          h.assert_true(
            diags.size() >= 1,
            "expected at least 1 diagnostic for partial split")
          try
            let has_start_msg =
              diags(0)?.message.contains("start on the line after")
            h.assert_true(
              has_start_msg,
              "message should mention starting after '('")
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

class \nodoc\ _TestCallArgFmtMixedLayout is UnitTest
  """Some args sharing a line, others alone — flagged."""
  fun name(): String => "CallArgumentFormat: mixed layout flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(\n" +
      "      U32(1), U32(2),\n" +
      "      U32(3))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for mixed layout")
          try
            h.assert_true(
              diags(0)?.message.contains(
                "all be on one line or each on its own"),
              "message should mention layout requirement")
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

class \nodoc\ _TestCallArgFmtBothViolations is UnitTest
  """Arg on '(' line and mixed continuation layout — two diagnostics."""
  fun name(): String => "CallArgumentFormat: both violations"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    let x = Bar\n" +
      "    x.baz(U32(1),\n" +
      "      U32(2),\n" +
      "      U32(3), U32(4))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
          h.assert_eq[USize](
            2,
            diags.size(),
            "expected 2 diagnostics for both violations")
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestCallArgFmtFFIClean is UnitTest
  """FFI call with each arg on own line produces no diagnostics."""
  fun name(): String => "CallArgumentFormat: FFI clean"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "use @pony_os_errno[I32]()\n" +
      "\n" +
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    @pony_os_errno(\n" +
      "      U32(1),\n" +
      "      U32(2))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
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

class \nodoc\ _TestCallArgFmtFFIPartialSplit is UnitTest
  """FFI call with partial split — flagged."""
  fun name(): String => "CallArgumentFormat: FFI partial split flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "use @pony_os_errno[I32]()\n" +
      "\n" +
      "class Foo\n" +
      "  fun apply(): None =>\n" +
      "    @pony_os_errno(U32(1),\n" +
      "      U32(2))\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(mod, sf, lint.CallArgumentFormat)
          h.assert_true(
            diags.size() >= 1,
            "expected at least 1 diagnostic for FFI partial split")
          try
            h.assert_true(
              diags(0)?.message.contains("start on the line after"),
              "message should mention starting after '('")
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
