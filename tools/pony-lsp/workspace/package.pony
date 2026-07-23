use "collections"
use "files"
use "itertools"
use "json"

class val PackageData
  """
  Data obtained for a package during workspace scanning or ad-hoc when
  discovering new packages.

  Dependency information is only provided via workspace scanning when a
  `corral.json` file is involved.

  stdlib is an implicit dependency of all pony packages
  and is never listed here.
  """
  let name: String
  let folder: FilePath
  // dependencies, either full paths or repo names
  let dependencies: Array[String] val
  // absolute paths (derived from folder and dependencies)
  let dependency_paths: Array[String] val

  new val create(
    name': String,
    folder': FilePath,
    dependencies': Set[String] val = recover val Set[String].create() end)
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

  fun debug(): String =>
    """
    Return a JSON string representation for logging.
    """
    var dp_arr = JsonArray
    for dp in dependencies.values() do
      dp_arr = dp_arr.push(dp)
    end
    JsonObject
      .update("name", name)
      .update("folder", folder.path)
      .update("dependencies", dp_arr)
      .print()

primitive PackageDataBuilder
  """
  Build `PackageData` from folder path.

  This exists because we cannot call other constructors from a constructor.
  """
  fun tag from_path(folder: FilePath): PackageData =>
    let name = Path.base(folder.path)
    PackageData.create(name, folder)



