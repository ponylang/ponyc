use "files"

class val WorkspaceFolders
  let folders: Array[FilePath] val

  new val create(file_auth: FileAuth, workspace_folders: Array[String] val) =>
    this.folders =
      recover
        let arr = Array[FilePath].create(workspace_folders.size())
        for s in workspace_folders.values() do
          try
            let folder = FilePath(file_auth, s).canonical()?
            arr.push(folder)
          end
        end
        arr
      end

  fun box contains(document_path: String): Bool =>
    """
    Returns `true` if `document_path` is in any subdirectory of any of the
    workspace folders
    """

    var dir_path = Path.dir(document_path)
    while (dir_path != ".") do
      for folder in this.folders.values() do
        if dir_path == folder.path then
          return true
        end
      end
      dir_path = Path.dir(dir_path)
    end
    false


