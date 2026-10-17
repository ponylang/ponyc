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
    corral_dir: FilePath,
    workspace_name: String,
    visited: Set[String] ref = Set[String].create(),
    top_level_corral_dir: (FilePath | None) = None
  ): Array[ModuleData] val ? =>
    """
    Scan directory `dir`, that contains a `corral.json` file.
    Extract dependencies listed in `deps` in `corral.json`
    and their transitive dependencies.

    Return all found packages either listed in `corral.json` under `packages` or
    simply found because they are in this directory and contain pony files.

    Those packages all get the extracted dependencies applied.
    """
    let result = recover trn Array[ModuleData].create(2) end
    visited.set(corral_dir.path)

    // load corral.json if available
    let corral_json_path = corral_dir.join("corral.json")?
    let corral_json =
      if corral_json_path.exists() then
        _channel.log("corral.json @ " + corral_json_path.path)
        let corral_json_file = OpenFile(corral_json_path) as File
        let corral_json_str = corral_json_file.read_string(corral_json_file.size())
        match JSONParser.parse(consume corral_json_str)
        | let obj: JSONObject => obj
        else
          _channel.log("Invalid JSON @ " + corral_json_path.path)
          error
        end
      else
        _channel.log("No corral.json @ " + corral_dir.path)
        JSONObject
      end

    // extract modules from corral packages and dependencies
    let all_modules: Array[String] ref = Array[String].create(32)
    let module_paths = Set[String].create(all_modules.size())
    let json_pkgs = JSONPathParser.compile("$.packages.*")?.query(corral_json)
    for json_package in json_pkgs.values() do

      let package = json_package as String
      let pp = corral_dir.join(package)?
      Fact(pp.exists(), "Package path " + pp.path + " does not exist")?
      if not module_paths.contains(pp.path) then
        module_paths.set(pp.path)
        all_modules.push(package)
      end
    end
    // find additional mouldes inside `dir` not listed in corral.json packages
    // some projects don't list their packages
    let walk_handler =
      object is WalkHandler
        var pony_modules: Set[String] = Set[String].create(4)
        fun ref apply(
          dir_path: FilePath val,
          dir_entries: Array[String val] ref)
        =>
          let has_pony_file = Iter[String val](dir_entries.values())
              .any({(a) => Path.ext(a) == "pony" })
          if has_pony_file and not pony_modules.contains(dir_path.path) then
            pony_modules.set(dir_path.path)
          end
      end
    corral_dir.walk(walk_handler)
    //_channel.log("Walked")

    for mod in walk_handler.pony_modules.values() do
      if not module_paths.contains(mod) then
        let module_str = Path.rel(corral_dir.path, mod)?
        module_paths.set(mod)
        all_modules.push(module_str)
      end
    end

    let top_level_dir =
      try
        top_level_corral_dir as FilePath
      else
        corral_dir
      end

    // extract dependencies from corral.json deps field, including transitive ones
    let locators =
      JSONPathParser.compile("$.deps.*.locator")?.query(corral_json)
    let dependencies: Map[String, FilePath] iso = recover iso Map[String, FilePath].create(4) end
    for maybe_locator in locators.values() do
      try
        let locator = Locator(maybe_locator as String)
        let locator_dir =
          if locator.is_local_direct() then
            // locate packages relative to this corral directory
            corral_dir.join(locator.path())?
          else
            let locator_flat_name = locator.flat_name()
            // TODO: trigger a corral fetch command
            // when _corral dir is not yet there
            //
            // resolve dependency paths relative to the root corral dir
            // as this is where corral is putting all aggregated top-level dependencies
            top_level_dir.join("_corral")?.join(locator_flat_name)?
          end
        _channel.log("Locator: " + locator_dir.path)
        _channel.log("Visited: " + ", ".join(visited.values()))
        if not visited.contains(locator_dir.path) then
          dependencies.insert_if_absent(locator_dir.path, locator_dir)
          visited.set(locator_dir.path)

          // scan for transitive dependencies but only if we havent visited
          // before to avoid endless loops over cyclic dependencies
          let corral_dependency_packages =
            this._scan_corral_dir(
              locator_dir,
              workspace_name,
              visited,
              top_level_dir
          )?
          _channel.log("Locator Packages: " + ",".join(corral_dependency_packages.values()))
          for sub_package in corral_dependency_packages.values() do
            for sub_dependency in sub_package.dependencies.values() do
              dependencies.insert_if_absent(sub_dependency.path, sub_dependency)
            end
          end
        end
      end
    end
    let num_deps = dependencies.size()
    let dependencies_val: Array[FilePath] val =
      recover val
        Iter[FilePath]((consume dependencies).values()).collect(Array[FilePath].create(num_deps))
      end
    // _channel.log("DEPENDENCIES: " + ", ".join(dependencies_val.values()))
    _channel.log("ALL MODULES: " + ", ".join(all_modules.values()))
    for module in all_modules.values() do
      try
        let mdir = corral_dir.join(module)?
        let name =
          if module.at("_corral" + Path.sep()) then
            // strip _corral prefix
            let path_elems: Array[String] val = module.split_by(Path.sep())
            let skip: USize = if path_elems.size() > 2 then
              2
            else
              1
            end
            Path.sep().join(Iter[String](path_elems.values()).skip(skip))
          else
            module
          end
        result.push(ModuleData(
          name,
          mdir,
          dependencies_val
        ))
      end
    end
    result

  fun scan(
    folder: FilePath,
    workspace_name: (String | None) = None
  ): Array[ModuleData] val =>
    """
    Scan a folder for Pony packages.

    A package is a directory with .pony files in it.

    A corral.json helps discover packages and configure their dependencies for
    successful compilation.
    """
    let name =
      match \exhaustive\ workspace_name
      | let n: String => n
      | None => Path.base(folder.path)
      end
    let that: WorkspaceScanner box = this
    let handler =
      object is WalkHandler
        var modules: Map[String, ModuleData] iso =
          Map[String, ModuleData].create(1)
        let visited: Set[String] ref = Set[String].create(32)

        fun ref apply(
          dir_path: FilePath val,
          dir_entries: Array[String val] ref)
        =>
          let is_corral = {(a: String val): Bool => a == "corral.json" }
          let has_pony_ext = {(a: String val): Bool => Path.ext(a) == "pony" }
          // first look for a `corral.json` file
          if not visited.contains(dir_path.path) then
            if Iter[String val](dir_entries.values()).any(is_corral) then
              try
                let corral_packages = that._scan_corral_dir(
                  dir_path,
                  name
                  where
                  visited = visited
                )?
                _channel.log("Corral Packages: " + ", ".join(corral_packages.values()))
                for module in corral_packages.values() do
                  // let corral data overwrite other stuff
                  modules.insert(module.folder.path, module)
                end
              end
              // if no `corral.json` could be found, search for `.pony` files
            elseif Iter[String val](dir_entries.values()).any(has_pony_ext) then
              try
                // not inside a known workspace
                let main_workspace_name: String =
                  if dir_path.path == folder.path then
                    Path.base(dir_path.path)
                  else
                    Path.rel(folder.path, dir_path.path)?
                  end
                let module = ModuleData(main_workspace_name, dir_path)
                modules.insert_if_absent(
                  dir_path.path,
                  module
                )
              end
            end
          end
      end
    folder.walk(handler)

    let modules = handler.modules = Map[String, ModuleData].create()
    _channel.log(modules.size().string() + " Modules found in " + folder.path)
    let num_modules = modules.size()
    let result =
      recover val
        Iter[ModuleData]((consume modules).values())
          .collect(Array[ModuleData].create(num_modules))
      end
    for p in result.values() do
      _channel.log("Found package: " + p.debug())
    end
    result
