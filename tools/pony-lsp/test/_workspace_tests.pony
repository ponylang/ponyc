use "pony_test"
use "json"
use "files"
use ".."
use "../workspace"

primitive \nodoc\ _WorkspaceTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_RouterFindTest)
    test(_ModuleStateRemoveDocumentTest)
    test(_ModuleStateRemoveDocumentMissingTest)
    test(_ModuleStateDocumentPathsTest)
    test(_ModuleStateDocumentStatesTest)


class \nodoc\ iso _ModuleStateRemoveDocumentTest is UnitTest
  fun name(): String => "module_state/remove_document"

  fun apply(h: TestHelper) ? =>
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, "/fake/path")
    let mod_data = ModuleData.create("fake", path)
    let mod = ModuleState.create(mod_data, FakeChannel)
    let doc_path = "/fake/path/main.pony"
    mod.ensure_document(doc_path)
    h.assert_true(mod.has_document(doc_path))
    mod.remove_document(doc_path)?
    h.assert_false(mod.has_document(doc_path))

class \nodoc\ iso _ModuleStateRemoveDocumentMissingTest is UnitTest
  fun name(): String => "module_state/remove_document/missing"

  fun apply(h: TestHelper) =>
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, "/fake/path")
    let mod_data = ModuleData.create("fake", path)
    let mod = ModuleState.create(pkg_data, FakeChannel)
    var errored = false
    try
      mod.remove_document("/fake/path/missing.pony")?
    else
      errored = true
    end
    h.assert_true(errored)

class \nodoc\ iso _ModuleStateDocumentPathsTest is UnitTest
  fun name(): String => "module_state/document_paths"

  fun apply(h: TestHelper) =>
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, "/fake/path")
    let mod_data = ModuleData.create("fake", path)
    let mod = ModuleState.create(mod_data, FakeChannel)
    mod.ensure_document("/fake/path/a.pony")
    mod.ensure_document("/fake/path/b.pony")
    let paths = Array[String]
    for p in mod.document_paths() do
      paths.push(p)
    end
    h.assert_eq[USize](2, paths.size())
    h.assert_array_eq_unordered[String](
      ["/fake/path/a.pony"; "/fake/path/b.pony"],
      paths)

class \nodoc\ iso _ModuleStateDocumentStatesTest is UnitTest
  fun name(): String => "module_state/document_states"

  fun apply(h: TestHelper) =>
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, "/fake/path")
    let mod_data = ModuleData.create("fake", path)
    let mod = ModuleState.create(mod_data, FakeChannel)
    mod.ensure_document("/fake/path/a.pony")
    mod.ensure_document("/fake/path/b.pony")
    let state_paths = Array[String]
    for s in mod.document_states() do
      state_paths.push(s.path)
    end
    h.assert_eq[USize](2, state_paths.size())
    h.assert_array_eq_unordered[String](
      ["/fake/path/a.pony"; "/fake/path/b.pony"],
      state_paths)

class tag FakeRequestSender is RequestSender
  """
  Fake request sender for testing.
  """
  new tag create() => None

  fun tag send_request(
    method: String val,
    params: (JSONObject | JSONArray | None),
    notify: (ResponseNotify | None) = None)
  =>
    None

actor FakeChannel is Channel
  """
  Fake communication channel for testing.
  """
  be send(msg: Message val) =>
    None

  be log(data: String val, message_type: MessageType = Debug) =>
    None

  be set_notifier(notifier: Notifier tag) =>
    None

  be dispose() =>
    None
