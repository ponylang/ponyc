use "collections"
use "files"
use "pony_test"
use ast = "pony_compiler"

class \nodoc\ _TestDefaultValueCallExpr is UnitTest
  """
  Verifies that `ISize.max_value()` is extracted as source text, not as
  the internal token keyword "call".
  """
  fun name(): String => "DefaultValue/call-expr"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n  fun apply(n: ISize = ISize.max_value()): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      h.assert_eq[String](defaults(0)?, "ISize.max_value()")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueIntLiteral is UnitTest
  fun name(): String => "DefaultValue/int-literal"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n  fun apply(n: USize = 42): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      h.assert_eq[String](defaults(0)?, "42")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueStringLiteral is UnitTest
  fun name(): String => "DefaultValue/string-literal"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n  fun apply(s: String = \"hi\"): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      h.assert_eq[String](defaults(0)?, "\"hi\"")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueNegation is UnitTest
  """
  The sugar pass transforms `-1` to `1.neg()`, but source extraction
  should return the original `-1`.
  """
  fun name(): String => "DefaultValue/negation"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n  fun apply(x: ISize = -1): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      h.assert_eq[String](defaults(0)?, "-1")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueMultipleParams is UnitTest
  fun name(): String => "DefaultValue/multiple-params"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n" +
      "  fun apply(a: USize = 1, b: String = \"x\"," +
      " c: ISize = ISize.max_value()): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 3)
      h.assert_eq[String](defaults(0)?, "1")
      h.assert_eq[String](defaults(1)?, "\"x\"")
      h.assert_eq[String](defaults(2)?, "ISize.max_value()")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueBoolLiteral is UnitTest
  fun name(): String => "DefaultValue/bool-literal"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n  fun apply(x: Bool = true): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      h.assert_eq[String](defaults(0)?, "true")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueFloatLiteral is UnitTest
  fun name(): String => "DefaultValue/float-literal"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n  fun apply(x: F64 = 3.14): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      h.assert_eq[String](defaults(0)?, "3.14")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueNoneValue is UnitTest
  fun name(): String => "DefaultValue/none-value"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n" +
      "  fun apply(x: (String | None) = None): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      h.assert_eq[String](defaults(0)?, "None")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueRecoverBlock is UnitTest
  """
  A `recover` block default spans multiple tokens. This is the case
  from issue #1262 where the old docgen showed just "recover".
  """
  fun name(): String => "DefaultValue/recover-block"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n" +
      "  fun apply(s: String val =" +
      " recover val String end): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      h.assert_eq[String](defaults(0)?, "recover val String end")
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueMultiLine is UnitTest
  """
  A triple-quoted string default spans multiple lines. The source
  extraction must track newlines within the token to find the correct
  end position.
  """
  fun name(): String => "DefaultValue/multi-line"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n" +
      "  fun apply(s: String = \"\"\"\n" +
      "    hello\n" +
      "    \"\"\"): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 1)
      let extracted = defaults(0)?
      h.assert_true(
        extracted.contains("hello"),
        "extracted should contain 'hello', got: " + extracted)
    else
      h.fail("extraction failed")
    end

class \nodoc\ _TestDefaultValueNone is UnitTest
  """
  Parameters without a default should produce None, not appear in results.
  """
  fun name(): String => "DefaultValue/no-default"

  fun apply(h: TestHelper) =>
    let source =
      "primitive Foo\n  fun apply(n: USize): None => None\n"
    try
      let defaults = _DefaultValueTestHelper.extract_defaults(h, source)?
      h.assert_eq[USize](defaults.size(), 0)
    else
      h.fail("extraction failed")
    end

primitive \nodoc\ _DefaultValueTestHelper
  """
  Compiles Pony source through PassTraits and extracts default parameter
  values from the original source text via `source_span()`.
  """
  fun extract_defaults(
    h: TestHelper,
    source: String val)
    : Array[String val] ?
  =>
    let program = _compile(h, source)?
    let defaults = Array[String val]
    let pkgs = program.packages()
    if pkgs.has_next() then
      try
        let pkg = pkgs.next()?
        for module in pkg.modules() do
          for entity in module.ast.children() do
            if ast.TokenIds.is_entity(entity.id()) then
              try
                let members = entity(4)?
                for member in members.children() do
                  match member.id()
                  | ast.TokenIds.tk_fun()
                  | ast.TokenIds.tk_new()
                  | ast.TokenIds.tk_be() =>
                    try
                      let params = member(3)?
                      for param in params.children() do
                        if param.id() == ast.TokenIds.tk_none() then
                          continue
                        end
                        try
                          let def_val = param(2)?
                          match _extract_one(def_val)
                          | let s: String val => defaults.push(s)
                          end
                        end
                      end
                    end
                  end
                end
              end
            end
          end
        end
      end
    end
    defaults

  fun _extract_one(def_val: ast.AST box): (String | None) =>
    if def_val.id() != ast.TokenIds.tk_none() then
      try
        let src = (def_val.source_contents() as String box)
        (let start_pos, let end_pos') = def_val.source_span()
        let start_offset = _pos_to_offset(src, start_pos)?
        var end_offset = _pos_to_offset(src, end_pos')?
        if end_offset >= start_offset then
          var extracted: String val =
            recover val
              src.substring(
                ISize.from[USize](start_offset),
                ISize.from[USize](end_offset + 1))
                .> strip()
            end

          var missing_ends = _count_unmatched_ends(extracted)
          while missing_ends > 0 do
            end_offset = _scan_to_next_end(src, end_offset + 1)?
            missing_ends = missing_ends - 1
          end

          if missing_ends == 0 then
            extracted =
              recover val
                src.substring(
                  ISize.from[USize](start_offset),
                  ISize.from[USize](end_offset + 1))
                  .> strip()
              end
          end
          extracted
        end
      end
    end

  fun _count_unmatched_ends(text: String val): USize =>
    let openers =
      [ as String:
        "recover"; "if"; "ifdef"; "iftype"; "match"; "while"; "for"
        "repeat"; "object"; "lambda"; "try"]
    var opens: USize = 0
    var ends: USize = 0
    var i: USize = 0
    let size = text.size()
    while i < size do
      try
        while (i < size) and not _is_word_char(text(i)?) do
          if text(i)? == '"' then
            i = i + 1
            if ((i + 1) < size) and (text(i)? == '"') and
              (text(i + 1)? == '"')
            then
              i = i + 2
              while (i + 2) < size do
                if (text(i)? == '"') and (text(i + 1)? == '"') and
                  (text(i + 2)? == '"')
                then
                  i = i + 3
                  break
                end
                i = i + 1
              end
            else
              while (i < size) and (text(i)? != '"') do
                i = i + 1
              end
              if i < size then i = i + 1 end
            end
          else
            i = i + 1
          end
        end
      end
      let word_start = i
      try
        while (i < size) and _is_word_char(text(i)?) do
          i = i + 1
        end
      end
      if i > word_start then
        let word: String val =
          text.substring(
            ISize.from[USize](word_start),
            ISize.from[USize](i))
        if word == "end" then
          ends = ends + 1
        else
          for opener in openers.values() do
            if word == opener then
              opens = opens + 1
              break
            end
          end
        end
      end
    end
    if opens > ends then opens - ends else 0 end

  fun _is_word_char(c: U8): Bool =>
    ((c >= 'a') and (c <= 'z')) or
    ((c >= 'A') and (c <= 'Z')) or
    ((c >= '0') and (c <= '9')) or
    (c == '_')

  fun _scan_to_next_end(
    src: String box,
    from: USize)
    : USize ?
  =>
    var i = from
    let size = src.size()
    while i < size do
      if not _is_word_char(src(i)?) then
        i = i + 1
      else
        let word_start = i
        while (i < size) and _is_word_char(src(i)?) do
          i = i + 1
        end
        if (src.substring(
          ISize.from[USize](word_start), ISize.from[USize](i)) == "end")
        then
          return i - 1
        end
      end
    end
    error

  fun _pos_to_offset(
    src: String box,
    p: ast.Position)
    : USize ?
  =>
    if p.line() == 1 then
      p.column() - 1
    else
      let line_idx = src.find("\n" where nth = p.line() - 2)?
      line_idx.usize() + p.column()
    end

  fun _get_ponypath(vars: Array[String val] val): String val =>
    for pair in vars.values() do
      if pair.at("PONYPATH=") then
        return pair.substring(ISize(9))
      end
    end
    ""

  fun _compile(
    h: TestHelper,
    source: String val)
    : ast.Program val ?
  =>
    let auth = h.env.root
    let tmp =
      FilePath.mkdtemp(
        FileAuth(auth), "pony-doc-test")?
    let pony_file =
      FilePath(
        FileAuth(auth), Path.join(tmp.path, "test.pony"))
    let file = File(pony_file)
    file.write(source)
    file.dispose()

    let pony_path = _get_ponypath(h.env.vars)

    let session =
      ast.CompileSession(
        tmp where package_search_paths = pony_path,
        limit = ast.PassParse)
    let result =
      try
        match session.program()
        | let program: ast.Program val =>
          if not session.continue_to(ast.PassTraits) then
            session.dispose()
            h.fail("compilation failed past parse")
            error
          end
          session.dispose()
          program
        else
          session.dispose()
          h.fail("compilation failed")
          error
        end
      else
        h.assert_true(tmp.remove(), "tmpdir cleanup failed")
        error
      end
    h.assert_true(tmp.remove(), "tmpdir cleanup failed")
    result
