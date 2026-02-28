use "pony_test"
use ast = "pony_compiler"
use lint = ".."

class \nodoc\ _TestCtrlAlignIfClean is UnitTest
  """Aligned if/elseif/else/end produces no diagnostics."""
  fun name(): String => "CtrlAlign: if/elseif/else/end aligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    if true then\n" +
      "      None\n" +
      "    elseif false then\n" +
      "      None\n" +
      "    else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignForClean is UnitTest
  """Aligned for/end produces no diagnostics."""
  fun name(): String => "CtrlAlign: for/end aligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    for x in [as U32: 1; 2].values() do\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignWhileClean is UnitTest
  """Aligned while/end produces no diagnostics."""
  fun name(): String => "CtrlAlign: while/end aligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    while false do\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignTryClean is UnitTest
  """Aligned try/else/then/end produces no diagnostics."""
  fun name(): String => "CtrlAlign: try/else/then/end aligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    try\n" +
      "      error\n" +
      "    else\n" +
      "      None\n" +
      "    then\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignRepeatClean is UnitTest
  """Aligned repeat/until/end produces no diagnostics."""
  fun name(): String => "CtrlAlign: repeat/until/end aligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    repeat\n" +
      "      None\n" +
      "    until true end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignWithClean is UnitTest
  """Aligned with/end produces no diagnostics."""
  fun name(): String => "CtrlAlign: with/end aligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "use \"files\"\n" +
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    with f = File(FilePath(FileAuth(env.root), \"x\")) do\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignIfdefClean is UnitTest
  """Aligned ifdef/elseif/else/end produces no diagnostics."""
  fun name(): String => "CtrlAlign: ifdef/elseif/else/end aligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    ifdef linux then\n" +
      "      None\n" +
      "    elseif windows then\n" +
      "      None\n" +
      "    else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignSingleLineSkipped is UnitTest
  """Single-line if is exempt from alignment checking."""
  fun name(): String => "CtrlAlign: single-line if skipped"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    if true then None end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignNestedClean is UnitTest
  """Nested structures each aligned independently produce no diagnostics."""
  fun name(): String => "CtrlAlign: nested structures aligned"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    if true then\n" +
      "      while false do\n" +
      "        None\n" +
      "      end\n" +
      "    else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
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

class \nodoc\ _TestCtrlAlignElseMisaligned is UnitTest
  """Else at wrong column produces a diagnostic."""
  fun name(): String => "CtrlAlign: misaligned else flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    if true then\n" +
      "      None\n" +
      "      else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned else")
          try
            h.assert_eq[String](
              "style/control-structure-alignment",
              diags(0)?.rule_id)
            h.assert_true(
              diags(0)?.message.contains("'else'"),
              "message should mention 'else'")
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

class \nodoc\ _TestCtrlAlignElseifMisaligned is UnitTest
  """Elseif at wrong column produces a diagnostic."""
  fun name(): String => "CtrlAlign: misaligned elseif flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    if true then\n" +
      "      None\n" +
      "      elseif false then\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned elseif")
          try
            h.assert_true(
              diags(0)?.message.contains("'elseif'"),
              "message should mention 'elseif'")
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

class \nodoc\ _TestCtrlAlignEndMisaligned is UnitTest
  """End at wrong column produces a diagnostic."""
  fun name(): String => "CtrlAlign: misaligned end flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    if true then\n" +
      "      None\n" +
      "      end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned end")
          try
            h.assert_true(
              diags(0)?.message.contains("'end'"),
              "message should mention 'end'")
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

class \nodoc\ _TestCtrlAlignForEndMisaligned is UnitTest
  """For with misaligned end produces a diagnostic."""
  fun name(): String => "CtrlAlign: for with misaligned end flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    for x in [as U32: 1; 2].values() do\n" +
      "      None\n" +
      "      end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned for end")
          try
            h.assert_true(
              diags(0)?.message.contains("'end'"),
              "message should mention 'end'")
            h.assert_true(
              diags(0)?.message.contains("'for'"),
              "message should mention 'for'")
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

class \nodoc\ _TestCtrlAlignTryElseMisaligned is UnitTest
  """Try with misaligned else produces a diagnostic."""
  fun name(): String => "CtrlAlign: try with misaligned else flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    try\n" +
      "      error\n" +
      "      else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned try else")
          try
            h.assert_true(
              diags(0)?.message.contains("'else'"),
              "message should mention 'else'")
            h.assert_true(
              diags(0)?.message.contains("'try'"),
              "message should mention 'try'")
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

class \nodoc\ _TestCtrlAlignRepeatUntilMisaligned is UnitTest
  """Repeat with misaligned until produces a diagnostic."""
  fun name(): String => "CtrlAlign: misaligned until flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    repeat\n" +
      "      None\n" +
      "      until true end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned until")
          try
            h.assert_true(
              diags(0)?.message.contains("'until'"),
              "message should mention 'until'")
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

class \nodoc\ _TestCtrlAlignWithEndMisaligned is UnitTest
  """With with misaligned end produces a diagnostic."""
  fun name(): String => "CtrlAlign: with misaligned end flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "use \"files\"\n" +
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    with f = File(FilePath(FileAuth(env.root), \"x\")) do\n" +
      "      None\n" +
      "      end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned with end")
          try
            h.assert_true(
              diags(0)?.message.contains("'end'"),
              "message should mention 'end'")
            h.assert_true(
              diags(0)?.message.contains("'with'"),
              "message should mention 'with'")
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

class \nodoc\ _TestCtrlAlignIfdefElseMisaligned is UnitTest
  """Ifdef with misaligned else produces a diagnostic."""
  fun name(): String => "CtrlAlign: ifdef misaligned else flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    ifdef linux then\n" +
      "      None\n" +
      "      else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned ifdef else")
          try
            h.assert_true(
              diags(0)?.message.contains("'else'"),
              "message should mention 'else'")
            h.assert_true(
              diags(0)?.message.contains("'ifdef'"),
              "message should mention 'ifdef'")
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

class \nodoc\ _TestCtrlAlignMultipleMisaligned is UnitTest
  """Both else and end misaligned produces two diagnostics."""
  fun name(): String => "CtrlAlign: multiple misalignments flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    if true then\n" +
      "      None\n" +
      "      else\n" +
      "      None\n" +
      "      end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            2,
            diags.size(),
            "expected 2 diagnostics for else + end misaligned")
        else
          h.fail("no module")
        end
      else
        h.fail("no package")
      end
    else
      h.fail("compilation failed")
    end

class \nodoc\ _TestCtrlAlignWhileElseMisaligned is UnitTest
  """While with misaligned else produces a diagnostic."""
  fun name(): String => "CtrlAlign: while misaligned else flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    while false do\n" +
      "      None\n" +
      "      else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned while else")
          try
            h.assert_true(
              diags(0)?.message.contains("'else'"),
              "message should mention 'else'")
            h.assert_true(
              diags(0)?.message.contains("'while'"),
              "message should mention 'while'")
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

class \nodoc\ _TestCtrlAlignForElseMisaligned is UnitTest
  """For with misaligned else produces a diagnostic."""
  fun name(): String => "CtrlAlign: for misaligned else flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    for x in [as U32: 1; 2].values() do\n" +
      "      None\n" +
      "      else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned for else")
          try
            h.assert_true(
              diags(0)?.message.contains("'else'"),
              "message should mention 'else'")
            h.assert_true(
              diags(0)?.message.contains("'for'"),
              "message should mention 'for'")
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

class \nodoc\ _TestCtrlAlignRepeatElseMisaligned is UnitTest
  """Repeat with misaligned else produces a diagnostic."""
  fun name(): String => "CtrlAlign: repeat misaligned else flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    repeat\n" +
      "      None\n" +
      "    until false\n" +
      "      else\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned repeat else")
          try
            h.assert_true(
              diags(0)?.message.contains("'else'"),
              "message should mention 'else'")
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

class \nodoc\ _TestCtrlAlignTryThenMisaligned is UnitTest
  """Try with misaligned then produces a diagnostic."""
  fun name(): String => "CtrlAlign: try misaligned then flagged"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let source: String val =
      "actor Main\n" +
      "  new create(env: Env) =>\n" +
      "    try\n" +
      "      error\n" +
      "    else\n" +
      "      None\n" +
      "      then\n" +
      "      None\n" +
      "    end\n"
    try
      (let program, let sf) = _ASTTestHelper.compile(h, source)?
      match program.package()
      | let pkg: ast.Package val =>
        match pkg.module()
        | let mod: ast.Module val =>
          let diags =
            _CollectRuleDiags(
              mod, sf, lint.ControlStructureAlignment)
          h.assert_eq[USize](
            1,
            diags.size(),
            "expected 1 diagnostic for misaligned try then")
          try
            h.assert_true(
              diags(0)?.message.contains("'then'"),
              "message should mention 'then'")
            h.assert_true(
              diags(0)?.message.contains("'try'"),
              "message should mention 'try'")
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
