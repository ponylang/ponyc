use "collections"
use "files"
use "itertools"
use "json"

class val WorkspaceData
  """
  Data extracted from corral.json
  """
  let name: String
  let folder: FilePath
  // dependencies, either full paths or repo names
  let dependencies: Array[String] val
  // absolute paths (derived from folder and dependencies)
  let dependency_paths: Array[String] val
  // absolute paths of modules listed in all
  // corral.json files (including dependencies)
  let module_paths: Set[String] val
    """
    Absolute paths of modules listed in all
    corral.json files (including dependencies).
    """
  let _min_module_paths_len: USize
  // TODO: further structure a workspace into different modules, separately
  // compiled

  new val create(
    name': String,
    folder': FilePath,
    dependencies': Set[String] val,
    module_paths': Set[String] val)
  =>
    name = name'
    folder = folder'
    dependencies =
      recover val
        Iter[String](dependencies'.values())
          .collect(Array[String].create(dependencies'.size()))
      end
    dependency_paths =
      recover val
        Iter[String](dependencies.values())
          .map[String](
            {(dep): String? =>
              if dep.contains(Path.sep()) then
                dep
              else
                folder.join("_corral")?.join(dep)?.path
              end
            })
          .collect(Array[String].create(dependencies.size()))
      end
    module_paths = module_paths'
    var min: USize = USize.max_value()
    for mod_path in module_paths.values() do
      min = min.min(mod_path.size())
    end
    _min_module_paths_len = min

  fun debug(): String =>
    """
    Return a JSON string representation for logging.
    """
    var dp_arr = JSONArray
    for dp in dependencies.values() do
      dp_arr = dp_arr.push(dp)
    end
    var pp_arr = JSONArray
    for pp in module_paths.values() do
      pp_arr = pp_arr.push(pp)
    end
    JSONObject
      .update("name", name)
      .update("folder", folder.path)
      .update("dependencies", dp_arr)
      .update("modules", pp_arr)
      .print()

  fun find_module(document_path: String): (FilePath | None) =>
    var doc_path: String val = document_path
    while (doc_path != ".") and (doc_path.size() >= _min_module_paths_len) do
      try
        let mod_path = this.module_paths(doc_path)?
        return this.folder.join(
          mod_path.substring(this.folder.path.size().isize() + 1)
        )?
      end
      let parent = Path.dir(doc_path)
      if parent == doc_path then
        break // hit filesystem root
      end
      doc_path = parent
    end
    None

  fun workspace_path(path: String): FilePath ? =>
    // check if path is within the workspace folder
    let found_idx =
      try
        path.find(this.folder.path)?
      else
        -1
      end
    if found_idx == 0 then
      this.folder.join(path.substring(this.folder.path.size().isize() + 1))?
    else
      error
    end
