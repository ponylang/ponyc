use "pony_test"
use "json"
use "files"
use ".."
use "../workspace"

primitive _WorkspaceTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_RouterFindTest)
    test(_PackageStateRemoveDocumentTest)
    test(_PackageStateRemoveDocumentMissingTest)
    test(_PackageStateDocumentPathsTest)
    test(_PackageStateDocumentStatesTest)

class \nodoc\ iso _RouterFindTest is UnitTest
  fun name(): String => "router/find"

  fun apply(h: TestHelper) ? =>
    let file_auth = FileAuth(h.env.root)
    let this_dir_path = Path.dir(__loc.file())
    let folder = FilePath(file_auth, this_dir_path)
    let channel = FakeChannel
    let scanner = WorkspaceScanner.create(channel)
    let workspaces = scanner.scan(file_auth, this_dir_path)
    h.assert_eq[USize](3, workspaces.size())

    // Verify all expected workspaces were found
    // (order is not guaranteed because path.walk
    // uses filesystem enumeration order)
    let actual = Array[String]
    for ws in workspaces.values() do
      actual.push(ws.folder.path)
    end
    let expected =
      [ as String:
        folder.path
        folder.join("error_workspace")?.path
        folder.join("workspace")?.path
      ]
    h.assert_array_eq_unordered[String](expected, actual)

    // Verify router lookup works
    let router = WorkspaceRouter.create()
    // dummy, not actually in use
    let compiler = PonyCompiler("")
    let request_sender = FakeRequestSender
    let client = Client.from(JsonObject)

    let mgr =
      WorkspaceManager(
        workspaces(0)?,
        file_auth,
        channel,
        request_sender,
        client,
        compiler)
    router.add_workspace(folder, mgr)?

    let file_path = folder.join("main.pony")?
    let found = router.find_workspace(file_path.path)
    h.assert_isnt[(WorkspaceManager | None)](None, found)

class \nodoc\ iso _PackageStateRemoveDocumentTest is UnitTest
  fun name(): String => "package_state/remove_document"

  fun apply(h: TestHelper) ? =>
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, "/fake/path")
    let pkg = PackageState.create(path, FakeChannel)
    let doc_path = "/fake/path/main.pony"
    pkg.ensure_document(doc_path)
    h.assert_true(pkg.has_document(doc_path))
    pkg.remove_document(doc_path)?
    h.assert_false(pkg.has_document(doc_path))

class \nodoc\ iso _PackageStateRemoveDocumentMissingTest is UnitTest
  fun name(): String => "package_state/remove_document/missing"

  fun apply(h: TestHelper) =>
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, "/fake/path")
    let pkg = PackageState.create(path, FakeChannel)
    var errored = false
    try
      pkg.remove_document("/fake/path/missing.pony")?
    else
      errored = true
    end
    h.assert_true(errored)

class \nodoc\ iso _PackageStateDocumentPathsTest is UnitTest
  fun name(): String => "package_state/document_paths"

  fun apply(h: TestHelper) =>
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, "/fake/path")
    let pkg = PackageState.create(path, FakeChannel)
    pkg.ensure_document("/fake/path/a.pony")
    pkg.ensure_document("/fake/path/b.pony")
    let paths = Array[String]
    for p in pkg.document_paths() do
      paths.push(p)
    end
    h.assert_eq[USize](2, paths.size())
    h.assert_array_eq_unordered[String](
      ["/fake/path/a.pony"; "/fake/path/b.pony"],
      paths)

class \nodoc\ iso _PackageStateDocumentStatesTest is UnitTest
  fun name(): String => "package_state/document_states"

  fun apply(h: TestHelper) =>
    let file_auth = FileAuth(h.env.root)
    let path = FilePath(file_auth, "/fake/path")
    let pkg = PackageState.create(path, FakeChannel)
    pkg.ensure_document("/fake/path/a.pony")
    pkg.ensure_document("/fake/path/b.pony")
    let state_paths = Array[String]
    for s in pkg.document_states() do
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
    params: (JsonObject | JsonArray | None),
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
