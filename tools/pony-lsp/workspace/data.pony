use "collections"
use "files"
use "itertools"
use "immutable-json"

class val WorkspaceData
  """
  Data extracted from corral.json
  """
  let name: String
  let folder: FilePath
  // dependency names, relative to <folder>/_corral/
  let dependencies: Array[String] val
  // absolute paths (derived from folder and dependencies)
  let dependency_paths: Array[String] val
  // absolute paths of packages listed in all corral.json files (including dependencies)
  let package_paths: Set[String] val
  let _min_package_paths_len: USize
  // TODO: further structure a workspace into different packages, separately
  // compiled

  new val create(
    name': String,
    folder': FilePath,
    dependencies': Set[String] val,
    package_paths': Set[String] val)
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
          .map[String]({(dep): String? => folder.join("_corral")?.join(dep)?.path})
          .collect(Array[String].create(dependencies.size()))
      end
    package_paths = package_paths'
    var min: USize = USize.max_value()
    for package_path in package_paths.values() do
      min = min.min(package_path.size())
    end
    _min_package_paths_len = min

  fun debug(): String =>
    var dp_arr = Arr.create()
    for dp in dependencies.values() do
      dp_arr = dp_arr(dp)
    end
    var pp_arr = Arr.create()
    for pp in package_paths.values() do
      pp_arr = pp_arr(pp)
    end
    Obj("name", name)(
      "folder", folder.path
    )(
      "dependencies", dp_arr
    )(
      "packages", pp_arr
    ).build().string()

  fun find_package(document_path: String): (FilePath | None) =>
    var doc_path: String val = document_path
    while (doc_path != ".") and (doc_path.size() >= _min_package_paths_len) do
      try
        let package_path = this.package_paths(doc_path)?
        return this.folder.join(
          package_path.substring(this.folder.path.size().isize() + 1)
        )?
      end
      doc_path = Path.dir(doc_path)
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
