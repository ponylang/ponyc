use "collections"
use "files"
use ".."

class WorkspaceRouter
  """
  Routes document URIs to the appropriate workspace manager.
  """
  let workspaces: Map[String, WorkspaceManager]
  var head_workspace: (WorkspaceManager | None)
  var tail_workspace: (WorkspaceManager | None)
  var min_workspace_path_len: USize

  new ref create() =>
    workspaces = Map[String, WorkspaceManager].create()
    head_workspace = None
    tail_workspace = None
    min_workspace_path_len = USize.max_value()

  fun handle_request_chained(
    file_uri: String,
    request: RequestMessage val,
    handler: {(WorkspaceManager tag, String val, RequestMessage val)} val)
  =>
    """
    Handle the provided notification for the file denoted by `file_uri` with the
    provided `handler`.

    First, the `file_uri` is checked, if it falls within any of the workspace
    folders. If so it is handled by this `WorkspaceManager`. If not it is
    reached down the chain of available workspaces and each on is checking if
    it has a matching module somewhere in the program.
    """
    match find_workspace(file_uri)
    | let mgr: WorkspaceManager =>
      handler(mgr, file_uri, request)
    else
      try
        (head_workspace as WorkspaceManager)
          .handle_request_chained(file_uri, request, handler)
      end
    end

  fun handle_notification_chained(
    file_uri: String,
    notification: Notification val,
    handler: {(WorkspaceManager tag, String val, Notification val)} val)
  =>
    """
    Handle the provided notification for the file denoted by `file_uri` with the
    provided `handler`.

    First, the `file_uri` is checked, if it falls within any of the workspace
    folders. If so it is handled by this `WorkspaceManager`. If not it is
    reached down the chain of available workspaces and each on is checking if
    it has a matching module somewhere in the program.
    """
    match this.find_workspace(file_uri)
    | let mgr: WorkspaceManager =>
      handler(mgr, file_uri, notification)
    else
      try
        (head_workspace as WorkspaceManager)
          .handle_notification_chained(file_uri, notification, handler)
      end
    end

  fun find_workspace(file_uri: String): (WorkspaceManager | None) =>
    """
    Find a workspace that contains this `file_uri`.

    This method only checks if `file_uri` is contained in the main folder
    of a workspace. It doesn't check if that workspace has the provided
    file_uri as one of its program modules.
    """
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
    // link this workspace to the next one
    // set the head, if we are the first
    if head_workspace is None then
      head_workspace = mgr
    end

    match tail_workspace
    | let last_mgr: WorkspaceManager =>
      last_mgr.set_next_workspace(mgr)
      mgr.set_prev_workspace(last_mgr)
    end
    // set this as the tail_workspace, to build a chain
    tail_workspace = mgr

    // insert this mgr into the known workspaces
    let abs_folder = folder.canonical()?.path
    let old_mgr = workspaces(abs_folder) = mgr
    match old_mgr
    | let old: WorkspaceManager =>
      // unlink the old one from the chain
      old.dispose()
    end
    this.min_workspace_path_len =
      this.min_workspace_path_len.min(abs_folder.size())

  fun ref dispose() =>
    for mgr in this.workspaces.values() do
      mgr.dispose()
    end
    this.workspaces.clear()
