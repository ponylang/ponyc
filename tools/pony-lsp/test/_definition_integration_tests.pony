use ".."
use "pony_test"
use "files"
use "json"
use "collections"

primitive _DefinitionIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _DefinitionLspServer(workspace_dir)
    test(_DefinitionClassIntegrationTest.create(server))
    test(_DefinitionThisIntegrationTest.create(server))
    test(_DefinitionKeywordsIntegrationTest.create(server))
    test(_DefinitionTraitIntegrationTest.create(server))
    test(_DefinitionUnionIntegrationTest.create(server))
    test(_DefinitionCrossFileIntegrationTest.create(server))
    test(_DefinitionGenericsIntegrationTest.create(server))
    test(_DefinitionTupleIntegrationTest.create(server))

class \nodoc\ iso _DefinitionClassIntegrationTest is UnitTest
  let _server: _DefinitionLspServer

  new iso create(server: _DefinitionLspServer) =>
    _server = server

  fun name(): String => "definition/integration/class"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_class.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // field usages → field declaration (line 4, "let" keyword span)
        ((7, 4), [("_class.pony", (4, 2), (4, 5))])
        ((10, 4), [("_class.pony", (4, 2), (4, 5))])
        // parameter usage → parameter declaration (line 6, "v: U32" span)
        ((7, 13), [("_class.pony", (6, 13), (6, 19))])
        // method call → method declaration (line 9, "fun" keyword span)
        ((13, 9), [("_class.pony", (9, 2), (9, 5))])
        // no definition on docstring content
        ((1, 4), [])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), _) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
    end
    _server.test_goto_definition(h, workspace_file, checks)

class \nodoc\ iso _DefinitionThisIntegrationTest is UnitTest
  let _server: _DefinitionLspServer

  new iso create(server: _DefinitionLspServer) =>
    _server = server

  fun name(): String => "definition/integration/this"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_class.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `this` in method body → enclosing class declaration (line 0)
        ((13, 4), [("_class.pony", (0, 0), (0, 5))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), _) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
    end
    _server.test_goto_definition(h, workspace_file, checks)

class \nodoc\ iso _DefinitionKeywordsIntegrationTest is UnitTest
  let _server: _DefinitionLspServer

  new iso create(server: _DefinitionLspServer) =>
    _server = server

  fun name(): String => "definition/integration/keywords"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_class.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `class` keyword in a declaration → no definition
        ((0, 0), [])
        // `new` keyword in a constructor declaration → no definition
        ((6, 2), [])
        // `fun` keyword in a method declaration → no definition
        ((9, 2), [])
        // `:` type annotation separator → no definition
        ((9, 11), [])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), _) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
    end
    _server.test_goto_definition(h, workspace_file, checks)

class \nodoc\ iso _DefinitionTraitIntegrationTest is UnitTest
  let _server: _DefinitionLspServer

  new iso create(server: _DefinitionLspServer) =>
    _server = server

  fun name(): String => "definition/integration/trait"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_trait.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // call via trait-typed receiver → trait method declaration (line 7)
        ((50, 6), [("_trait.pony", (7, 2), (7, 5))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), _) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
    end
    _server.test_goto_definition(h, workspace_file, checks)

class \nodoc\ iso _DefinitionUnionIntegrationTest is UnitTest
  let _server: _DefinitionLspServer

  new iso create(server: _DefinitionLspServer) =>
    _server = server

  fun name(): String => "definition/integration/union"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_trait.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // call via union-typed receiver → one definition per union member
        // _DefLeft.shared (line 29) and _DefRight.shared (line 35)
        ((53, 6),
          [
            ("_trait.pony", (29, 2), (29, 5))
            ("_trait.pony", (35, 2), (35, 5))
          ])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), _) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
    end
    _server.test_goto_definition(h, workspace_file, checks)

class \nodoc\ iso _DefinitionCrossFileIntegrationTest is UnitTest
  let _server: _DefinitionLspServer

  new iso create(server: _DefinitionLspServer) =>
    _server = server

  fun name(): String => "definition/integration/cross_file"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_cross_usage.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // type reference in parameter → class declaration in other file
        ((13, 16), [("_cross_target.pony", (0, 0), (0, 5))])
        // method call → method declaration in other file (line 13)
        ((14, 8), [("_cross_target.pony", (13, 2), (13, 5))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), _) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
    end
    _server.test_goto_definition(h, workspace_file, checks)

class \nodoc\ iso _DefinitionGenericsIntegrationTest is UnitTest
  let _server: _DefinitionLspServer

  new iso create(server: _DefinitionLspServer) =>
    _server = server

  fun name(): String => "definition/integration/generics"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_generics.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `T` in return type → type parameter declaration in method header
        ((19, 22), [("_generics.pony", (19, 12), (19, 16))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), _) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
    end
    _server.test_goto_definition(h, workspace_file, checks)

class \nodoc\ iso _DefinitionTupleIntegrationTest is UnitTest
  let _server: _DefinitionLspServer

  new iso create(server: _DefinitionLspServer) =>
    _server = server

  fun name(): String => "definition/integration/tuple"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_tuple.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `_1` tuple element access → `let pair` declaration
        ((16, 9), [("_tuple.pony", (15, 4), (15, 7))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), _) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
    end
    _server.test_goto_definition(h, workspace_file, checks)

type DefinitionExpectation is (String val, (I64, I64), (I64, I64))
type DefinitionCheck is ((I64, I64), Array[DefinitionExpectation] val)

class val _PendingDefinition
  let file_path: String
  let line: I64
  let character: I64
  let expected: Array[DefinitionExpectation] val
  let h: TestHelper
  let action: String

  new val create(
    file_path': String,
    line': I64,
    character': I64,
    expected': Array[DefinitionExpectation] val,
    h': TestHelper,
    action': String)
  =>
    file_path = file_path'
    line = line'
    character = character'
    expected = expected'
    h = h'
    action = action'

actor _DefinitionLspServer is Channel
  """
  Shared LSP server for all definition workspace tests.
  Initializes and compiles once, then dispatches
  individual definition requests to each test's TestHelper.
  """
  let _workspace_dir: String
  var _server: (BaseProtocol | None)
  var _ready: Bool
  var _initialized: Bool
  let _pending: Array[_PendingDefinition]
  let _opened: Set[String]
  let _in_flight: Map[I64, _PendingDefinition]
  var _next_id: I64

  new create(workspace_dir: String) =>
    _workspace_dir = workspace_dir
    _server = None
    _ready = false
    _initialized = false
    _pending = Array[_PendingDefinition]
    _opened = Set[String]
    _in_flight = Map[I64, _PendingDefinition]
    _next_id = 2

  be test_goto_definition(
    h: TestHelper,
    workspace_file: String,
    checks: Array[DefinitionCheck] val)
  =>
    let file_path = Path.join(_workspace_dir, workspace_file)
    for ((line, character), expected) in checks.values() do
      let action: String val =
        recover
          val workspace_file + ":" + line.string() + ":" + character.string()
        end
      let pending =
        _PendingDefinition(file_path, line, character, expected, h, action)
      if _ready then
        if not _opened.contains(file_path) then
          _opened.set(file_path)
          _did_open(file_path)
        end
        _dispatch(pending)
      else
        _pending.push(pending)
      end
    end
    if not _initialized then
      _initialized = true
      let ponyc =
        try
          h.env.args(0)?
        else
          ""
        end
      let proto =
        BaseProtocol(LanguageServer(this, h.env, PonyCompiler("", ponyc)))
      _server = proto
      proto(LspMsg.initialize(_workspace_dir))
    end

  fun ref _dispatch(pending: _PendingDefinition) =>
    let id = _next_id
    _next_id = id + 1
    try
      (_server as BaseProtocol)(
        RequestMessage(
          id,
          Methods.text_document().definition(),
          JsonObject
            .update(
              "textDocument",
              JsonObject.update("uri", Uris.from_path(pending.file_path)))
            .update(
              "position",
              JsonObject
                .update("line", pending.line)
                .update("character", pending.character))
        ).into_bytes()
      )
      _in_flight(id) = pending
    else
      pending.h.fail_action(pending.action)
    end

  be send(msg: Message val) =>
    match msg
    | let res: ResponseMessage val =>
      try
        let id = res.id as RequestId
        if RequestIds.eq(id, I64(0)) then
          try
            (_server as BaseProtocol)(LspMsg.initialized())
          end
        else
          try
            let id_i64 = id as I64
            (_, let pending) = _in_flight.remove(id_i64)?
            var ok = true
            let got_count =
              try JsonNav(res.result).size()? else 0 end
            if not pending.h.assert_eq[USize](
              pending.expected.size(),
              got_count,
              "Wrong number of definitions")
            then
              ok = false
            end
            for (i, loc) in pending.expected.pairs() do
              (let file_suffix, let start_pos, let end_pos) = loc
              (let exp_start_line, let exp_start_char) = start_pos
              (let exp_end_line, let exp_end_char) = end_pos
              try
                let nav = JsonNav(res.result)(i)
                let uri = nav("uri").as_string()?
                let got_start_line =
                  nav("range")("start")("line").as_i64()?
                let got_start_char =
                  nav("range")("start")("character").as_i64()?
                let got_end_line =
                  nav("range")("end")("line").as_i64()?
                let got_end_char =
                  nav("range")("end")("character").as_i64()?
                if not pending.h.assert_true(
                  uri.contains(file_suffix),
                  "Expected URI containing '" + file_suffix + "', got: " + uri)
                then
                  ok = false
                end
                if not pending.h.assert_eq[I64](
                  exp_start_line,
                  got_start_line)
                then
                  ok = false
                end
                if not pending.h.assert_eq[I64](
                  exp_start_char,
                  got_start_char)
                then
                  ok = false
                end
                if not pending.h.assert_eq[I64](
                  exp_end_line,
                  got_end_line)
                then
                  ok = false
                end
                if not pending.h.assert_eq[I64](
                  exp_end_char,
                  got_end_char)
                then
                  ok = false
                end
              else
                ok = false
                pending.h.log(
                  "Definition [" + i.string() +
                  "] returned null or invalid, expected (" +
                  file_suffix + ":" + exp_start_line.string() +
                  ":" + exp_start_char.string() + ")")
              end
            end
            if ok then
              pending.h.complete_action(pending.action)
            else
              pending.h.fail_action(pending.action)
            end
          end
        end
      end
    | let req: RequestMessage val =>
      if req.method == Methods.workspace().configuration() then
        try
          let proto = _server as BaseProtocol
          proto(ResponseMessage(req.id, JsonArray).into_bytes())
          for p in _pending.values() do
            if not _opened.contains(p.file_path) then
              _opened.set(p.file_path)
              _did_open(p.file_path)
            end
          end
        end
      end
    | let n: Notification val =>
      if n.method == Methods.text_document().publish_diagnostics() then
        if not _ready then
          _ready = true
          for p in _pending.values() do
            _dispatch(p)
          end
          _pending.clear()
        end
      end
    end

  fun ref _did_open(file_path: String) =>
    try
      (_server as BaseProtocol)(
        Notification(
          Methods.text_document().did_open(),
          JsonObject.update(
            "textDocument",
            JsonObject
              .update("uri", Uris.from_path(file_path))
              .update("languageId", "pony")
              .update("version", I64(1))
              .update("text", ""))
          ).into_bytes())
    end

  be log(data: String val, message_type: MessageType = Debug) =>
    None

  be set_notifier(notifier: Notifier tag) =>
    None

  be dispose() =>
    None
