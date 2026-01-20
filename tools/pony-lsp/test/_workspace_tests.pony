use "pony_test"
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
    let channel = TestChannel(h,
      {(h: TestHelper, channel: TestChannel ref): Bool =>
        true
      })
    let scanner = WorkspaceScanner.create(channel)
    let workspaces = scanner.scan(file_auth, this_dir_path)
    h.assert_eq[USize](2, workspaces.size())
    // main.pony workspace has been found first
    var workspace = workspaces(0)?
    h.assert_eq[String](folder.path, workspace.folder.path)

    // corral workspace
    workspace = workspaces(1)?
    h.assert_eq[String](folder.join("workspace")?.path, workspace.folder.path)
    
    let router = WorkspaceRouter.create()
    let compiler = PonyCompiler("") // dummy, not actually in use
    
    let mgr = WorkspaceManager(workspace, file_auth, channel, compiler)
    router.add_workspace(folder, mgr)?

    let file_path = folder.join("main.pony")?
    let found = router.find_workspace(file_path.path)
    h.assert_isnt[(WorkspaceManager | None)](None, found)

