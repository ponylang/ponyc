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
