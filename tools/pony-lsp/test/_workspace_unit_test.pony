use "pony_test"
use "files"
use "collections"

trait WsUnitTest
  """
  Workspace Unit Test.

  A Unit Test that sets up its own workspace in a temporary directory.

  The workspace directory is passed to the test function.

  Wrapped into a `WsTest` for `PonyTest` compatibility.
  """
  fun setup_workspace(workspace_builder: WorkspaceBuilder ref)
    """
    Use the `workspace_builder` to setup the workspace files and
    directories.
    """
  fun name(): String
    """Test name, same as in pony_test"""
  fun ref set_up(h: TestHelper val) ? => None
  fun apply(workspace: FilePath, h: TestHelper) ?
    """Run the test case"""
  fun ref tear_down(h: TestHelper val) => None


class WsTest is UnitTest
  """
  Workspace Test harness, running a [WsUnitTest](pony-lsp-WsUnitTest.md).

  Handles the creation of the workspace directory before executing the test
  and the deletion right afterwards.
  """
  let _test: WsUnitTest
  let _ws: WorkspaceBuilder = WorkspaceBuilder
  var _ws_dir: (FilePath | None) = None

  new create(test: WsUnitTest) =>
    _test = test
    _test.setup_workspace(_ws)

  fun name(): String => _test.name()

  fun ref set_up(h: TestHelper val) ? =>
    // setup temporary directory and store reference to it
    this._ws_dir = _ws.build(FileAuth(h.env.root))?
    this._test.set_up(h)?

  fun apply(h: TestHelper) ? =>
    let ws =
      try
        _ws_dir as FilePath
      else
        h.log("Workspace dir was not created properly")
        error
      end
    this._test.apply(ws, h)?

  fun ref tear_down(h: TestHelper val) =>
    this._test.tear_down(h)
    _ws.dispose() // remove temporary directory

class ref WorkspaceBuilder
  """
  Used to build workspace directory definitions, including directories, files
  and their contents.

  The workspace is built inside a random temporary directory.
  """
  let _dir_builders: Array[WorkspaceDirBuilder] = _dir_builders.create()
  let _files: Map[String, ByteSeq] = _files.create()

  var _workspace_dir: (Directory box | None box) = None

  new ref create() => None

  fun ref dir(rel_path: String): WorkspaceDirBuilder ref =>
    """
    Create a directory.

    Returns a [WorkspaceDirBuilder](pony-lsp-WorkspaceDirBuilder.md),
    which can be used to add files and subdirectories.
    """
    let builder = WorkspaceDirBuilder(rel_path)
    _dir_builders.push(builder)
    builder

  fun ref file(file_name: String, content: ByteSeq): WorkspaceBuilder ref =>
    """
    Add a file directly to the workspace directory.

    Returns this instance itself for further use in method-chaining style.
    """
    this._files.insert(file_name, content)
    this

  fun ref build(auth: FileAuth): FilePath? =>
    """
    Build the workspace inside a random temporary directory.

    Returns the path to that random temporary workspace directory.
    """
    let workspace_dir: Directory ref = Directory.create(
      FilePath.mkdtemp(auth, "workspace_builder")?)?
    this._workspace_dir = workspace_dir
    // write dir-local files
    for (file_name, content) in this._files.pairs() do
      let fd = workspace_dir.create_file(file_name)?
      fd.write(content)
    end
    // build dir builders
    for dir_builder in this._dir_builders.values() do
      dir_builder.build(workspace_dir)?
    end
    workspace_dir.path

  fun ref dispose() =>
    """
    Recursively deletes the workspace directory.
    """
    match this._workspace_dir
    | let d: Directory box => d.path.remove()
    end


class ref WorkspaceDirBuilder
  let _path: String
  let _dir_builders: Array[WorkspaceDirBuilder] = _dir_builders.create()
  let _files: Map[String, ByteSeq] = _files.create(2)

  new ref create(path: String) =>
    _path = path

  fun ref dir(rel_path: String): WorkspaceDirBuilder ref =>
    let builder = WorkspaceDirBuilder(rel_path)
    _dir_builders.push(builder)
    builder

  fun ref file(file_name: String, content: ByteSeq): WorkspaceDirBuilder ref =>
    this._files.insert(file_name, content)
    this

  fun build(parent_dir: Directory box)? =>
    // create this dir
    if not parent_dir.mkdir(this._path) then
      error
    end
    // open this dir
    let this_dir: Directory ref = parent_dir.open(this._path)?
    // populate local files
    for (file_name, content) in this._files.pairs() do
      let fd = this_dir.create_file(file_name)?
      fd.write(content)
    end

    // build sub dir builders
    for dir_builder in this._dir_builders.values() do
      dir_builder.build(this_dir)?
    end

primitive PonyMainSrc
  fun apply(): String =>
    """actor Main new create(env: Env) => None"""
