use ".."
use "assert"
use "collections"
use "files"
use "itertools"
use "json"

class WorkspaceScanner
  let _channel: Channel

  new val create(channel: Channel) =>
    _channel = channel

  fun _scan_corral_dir(
    dir: FilePath,
    workspace_name: String,
    visited: Set[String] ref = Set[String].create()
  ): WorkspaceData ? =>
    visited.set(dir.path)

    // load corral.json if available
    let corral_json_path = dir.join("corral.json")?
    _channel.log("corral.json @ " + corral_json_path.path)
    let corral_json_file = OpenFile(corral_json_path) as File
    let corral_json_str = corral_json_file.read_string(corral_json_file.size())
    let corral_json =
      match JsonParser.parse(consume corral_json_str)
      | let obj: JsonObject => obj
      else error
      end

    // extract packages
    let packages = JsonPathParser.compile("$.packages.*")?.query(corral_json)
    let package_paths = recover trn Set[String].create(2) end
    for package in packages.values() do
      let pp =
        try
          dir.join(package as String)?
        else
          _channel.log("packages element not a string")
          error
        end
      Fact(pp.exists(), "Package path " + pp.path + " does not exist")?
      package_paths.set(pp.path)
    end

    // extract dependencies, also transitive ones
    let locators = JsonPathParser.compile("$.deps.*.locator")?.query(corral_json)
    let dependencies = recover trn Set[String].create(4) end
    for locator in locators.values() do
      try
        let locator_flat_name = Locator(locator as String).flat_name()
        let locator_dir = dir.join("_corral")?.join(locator_flat_name)?

        let already_in = visited.contains(locator_dir.path)
        if not already_in then
          dependencies.set(locator_flat_name)
          visited.set(locator_dir.path)
          try
            // scan for transitive dependencies
            // but only if we havent visited before
            // to avoid endlees loops over cyclic dependencies
            let sub_workspace = this._scan_corral_dir(locator_dir, workspace_name, visited)?
            for sub_dependency in sub_workspace.dependencies.values() do
              dependencies.set(sub_dependency)
            end
          end
        end
      end
    end
    WorkspaceData(workspace_name, dir, consume dependencies, consume package_paths)

  fun scan(auth: FileAuth, folder: String, workspace_name: (String | None) = None): Array[WorkspaceData] val =>
    let path = FilePath(auth, folder)
    let name =
      match workspace_name
      | let n: String => n
      | None => Path.base(folder)
      end
    let that: WorkspaceScanner box = this
    let handler = object is WalkHandler
      var workspaces: Array[WorkspaceData] trn = recover trn Array[WorkspaceData].create(1) end

      fun ref apply(dir_path: FilePath val, dir_entries: Array[String val] ref) =>
        try
          // skip over _corral folders
          let idx = dir_entries.find("_corral" where predicate = {(a,b) => a == b})?
          dir_entries.delete(idx)?
        end
        // first look for a `corral.json` file
        try
          dir_entries.find("corral.json" where predicate = {(a,b) => a == b})?
          let workspace = that._scan_corral_dir(dir_path, name)?
          _channel.log("Added workspace: " + workspace.debug())
          workspaces.push(workspace)
        end
        // if no `corral.json` could be found, search for a `main.pony`
        try
          // ensure we are not inside a folder that is alrady in a workspace,
          // those are scanned
          dir_entries.find("main.pony" where predicate = {(a,b) => a == b})?
          try
            Iter[WorkspaceData](workspaces.values()).find({(workspace: WorkspaceData) =>
              dir_path.path.at(workspace.folder.path, 0)
            })?
          else
            // not inside a known workspace
            let main_workspace_name: String = dir_path.path.substring((folder.size() + 1).isize())
            workspaces.push(
              WorkspaceData(main_workspace_name, dir_path, Set[String].create(0), Set[String].create(0))
            )
          end
        end

    end
    path.walk(handler)
    let workspaces = handler.workspaces = []
    _channel.log(workspaces.size().string() + " Workspaces found in " + folder)
    consume workspaces
