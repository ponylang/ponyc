use "collections"
use "files"
use "json"
use ".."

class WorkspaceRouter
  """
  Routes document URIs to the appropriate workspace manager.
  """
  let workspaces: Map[String, WorkspaceManager]
  var min_workspace_path_len: USize

  new ref create() =>
    workspaces = Map[String, WorkspaceManager].create()
    min_workspace_path_len = USize.max_value()

  fun find_workspace(file_uri: String): (WorkspaceManager | None) =>
    var file_path = Uris.to_path(file_uri)
    // check the parent directories upwards if any
    // of them is part of a workspace
    while (file_path != ".") and
      (file_path.size() >= this.min_workspace_path_len)
    do
      try
        let workspace = this.workspaces(file_path)?
        return workspace
      end
      let parent = Path.dir(file_path)
      if parent == file_path then
        break // hit filesystem root
      end
      file_path = parent
    end
    None

  fun ref add_workspace(folder: FilePath, mgr: WorkspaceManager) ? =>
    """
    Register a workspace manager for a folder.
    """
    let abs_folder = folder.canonical()?.path
    let old_mgr = workspaces(abs_folder) = mgr
    match old_mgr
    | let old: WorkspaceManager => old.dispose()
    end
    this.min_workspace_path_len =
      this.min_workspace_path_len.min(abs_folder.size())

  fun workspace_symbol(
    query: String,
    channel: Channel,
    request_id: RequestId)
  =>
    """
    Broadcast a workspace/symbol request to all workspace managers.
    """
    let count = workspaces.size()
    if count == 0 then
      channel.send(ResponseMessage.create(request_id, JsonArray))
    else
      let agg = WorkspaceSymbolAggregator(channel, request_id, count)
      for mgr in workspaces.values() do
        mgr.workspace_symbol(query, agg)
      end
    end

  fun ref dispose() =>
    for mgr in this.workspaces.values() do
      mgr.dispose()
    end
    this.workspaces.clear()
