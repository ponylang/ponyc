use "collections"
use "files"
use "itertools"
use "json"

class val ModuleData is Comparable[ModuleData]
  """
  Data obtained for a module during workspace scanning or ad-hoc when
  discovering new modules.

  Dependency information is only provided via workspace scanning when a
  `corral.json` file is involved.

  stdlib is an implicit dependency of all pony modules
  and is never listed here.
  """
  let name: String
  let folder: FilePath
  // dependencies, full paths
  let dependencies: Array[FilePath] val

  new val create(
    name': String,
    folder': FilePath,
    dependencies': Array[FilePath] val = recover val Array[FilePath].create() end)
  =>
    name = name'
    folder = folder'
    dependencies = dependencies'

  fun debug(): String =>
    """
    Return a JSON string representation for logging.
    """
    var dp_arr = JSONArray
    for dp in dependencies.values() do
      dp_arr = dp_arr.push(dp.path)
    end
    JSONObject
      .update("name", name)
      .update("folder", folder.path)
      .update("dependencies", dp_arr)
      .print()


  fun string(): String iso^ =>
    debug().clone()

  fun eq(that: ModuleData val): Bool =>
    this.name.eq(that.name) and this.folder.path.eq(that.folder.path)
  fun lt(that: ModuleData val): Bool =>
    this.name.lt(that.name)


primitive ModuleDataBuilder
  """
  Build `PackageData` from folder path.

  This exists because we cannot call other constructors from a constructor.
  """
  fun tag from_path(folder: FilePath): ModuleData =>
    let name = Path.base(folder.path)
    ModuleData.create(name, folder)



