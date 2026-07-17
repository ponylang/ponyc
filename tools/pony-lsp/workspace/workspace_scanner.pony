use ".."
use "assert"
use "collections"
use "files"
use "itertools"
use "json"


class WorkspaceScanner
  """
  Scans directories to find Pony workspaces.
  """
  let _channel: Channel

  new val create(channel: Channel) =>
    _channel = channel

  fun _scan_corral_dir(
    dir: FilePath,
    workspace_name: String,
    visited: Set[String] ref = Set[String].create()
  ): Array[PackageData] val ? =>
    """
    Scan directory `dir`, that contains a `corral.json` file.
    Extract dependencies listed in `deps` in `corral.json` and their transitive dependencies.

    Return all found packages either listed in `corral.json` under `packages` or
    simply found because they are in this directory and contain pony files.

    Those packages all get the extracted dependencies applied.
    """
    let result = recover trn Array[PackageData].create(2) end
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
    let all_packages: Array[String] ref = Array[String].create(32)
    let package_paths = Set[String].create(all_packages.size())
    for json_package in JsonPathParser.compile("$.packages.*")?.query(corral_json).values() do
      let package = json_package as String
      let pp = dir.join(package)?
      Fact(pp.exists(), "Package path " + pp.path + " does not exist")?
      if not package_paths.contains(pp.path) then
        package_paths.set(pp.path)
        all_packages.push(package)
      end
    end
    // find additional packages inside `dir` not listed in corral.json packages
    // some projects don't list their packages
    let walk_handler =
      object is WalkHandler
        var pony_packages: Set[String] = Set[String].create(4)
        fun ref apply(
          dir_path: FilePath val,
          dir_entries: Array[String val] ref)
        =>
          let has_pony_file = Iter[String val](dir_entries.values())
              .any({(a) => Path.ext(a) == "pony" })
          if has_pony_file and not pony_packages.contains(dir_path.path) then
            pony_packages.set(dir_path.path)
          end
      end
    dir.walk(walk_handler)

    for pkg in walk_handler.pony_packages.values() do
      if not package_paths.contains(pkg) then
        let package_str = pkg.trim(dir.path.size())
        package_paths.set(pkg)
        all_packages.push(package_str)
      end
    end

    // extract dependencies, also transitive ones
    let locators =
      JsonPathParser.compile("$.deps.*.locator")?.query(corral_json)
    let dependencies =
      recover
        trn Set[String].create(4)
      end
    for maybe_locator in locators.values() do
      try
        let locator = Locator(maybe_locator as String)
        let locator_dir =
          if locator.is_local_direct() then
            dir.join(locator.path())?
          else
            let locator_flat_name = locator.flat_name()
            // TODO: trigger a corral fetch command when _corral dir is not yet there
            dir.join("_corral")?.join(locator_flat_name)?
          end

        let already_in = visited.contains(locator_dir.path)
        if not already_in then
          dependencies.set(locator_dir.path)
          visited.set(locator_dir.path)
          try
            // scan for transitive dependencies but only if we havent visited
            // before to avoid endless loops over cyclic dependencies
            let corral_dependency_packages =
              this._scan_corral_dir(locator_dir, workspace_name, visited)?
            for sub_package in corral_dependency_packages.values() do
              for sub_dependency in sub_package.dependencies.values() do
                dependencies.set(sub_dependency)
              end
            end
          end
        end
      end
    end
    let dependencies_val: Set[String] val = consume dependencies
    for package in all_packages.values() do
      try
        let pdir = dir.join(package)?
        result.push(PackageData(
          package,
          pdir,
          dependencies_val
        ))
      end
    end
    result

  fun scan(
    auth: FileAuth,
    folder: String,
    workspace_name: (String | None) = None
  ): Array[PackageData] val =>
    """
    Scan a folder for Pony packages.

    A package is a directory with .pony files in it.

    A corral.json helps discover packages and configure their dependencies for
    successful compilation.
    """
    let path = FilePath(auth, folder)
    let name =
      match \exhaustive\ workspace_name
      | let n: String => n
      | None => Path.base(folder)
      end
    let that: WorkspaceScanner box = this
    let handler =
      object is WalkHandler
        var packages: Map[String, PackageData] iso = Map[String, PackageData].create(1)

        fun ref apply(
          dir_path: FilePath val,
          dir_entries: Array[String val] ref)
        =>
          // first look for a `corral.json` file
          if Iter[String val](dir_entries.values()).any({(a) => a == "corral.json"}) then
            try
              for package in that._scan_corral_dir(dir_path, name)?.values() do
                // let corral data overwrite other stuff
                packages.insert(package.folder.path, package)
              end
            end
            // if no `corral.json` could be found, search for `.pony` files
          elseif Iter[String val](dir_entries.values()).any({(a) => Path.ext(a) == "pony" }) then
            // not inside a known workspace
            let main_workspace_name: String =
              if dir_path.path == folder then
                Path.base(dir_path.path)
              else
                dir_path.path.substring((folder.size() + 1).isize())
              end
            let package = PackageData(main_workspace_name, dir_path)
            packages.insert_if_absent(
              dir_path.path,
              package
            )
          end
      end
    path.walk(handler)
    
    let packages = handler.packages = Map[String, PackageData].create()
    _channel.log(packages.size().string() + " Packages found in " + folder)
    let agg = recover iso Array[PackageData].create(packages.size()) end
    let result = recover val Iter[PackageData]((consume packages).values()).collect(consume agg) end
    for p in result.values() do
      _channel.log("Found package: " + p.debug())
    end
    result

