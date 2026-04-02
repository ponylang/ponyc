use "files"

// pony-lint: allow style/acronym-casing
primitive Uris
  """
  URI conversion utilities for the language server.
  """

  fun to_path(uri: String): String =>
    """
    Convert an LSP document URI to a local filesystem
    path by stripping the scheme. On Windows, also strips
    the leading `/` before a drive letter (e.g.,
    `/D:/path` becomes `D:/path`) and normalizes forward
    slashes to the native separator.
    """
    let result = uri.split_by("://", 2)
    let path =
      try
        result(result.size() - 1)?
      else
        _Unreachable()
        return uri
      end
    ifdef windows then
      // file:///D:/path -> /D:/path after stripping
      // scheme; remove leading /
      let trimmed =
        try
          if (path.size() >= 3) and (path(0)? == '/')
            and (path(2)? == ':')
          then
            path.substring(1)
          else
            path
          end
        else
          path
        end
      let s = trimmed.clone()
      s.replace("/", "\\")
      consume s
    else
      path
    end

  fun from_path(path: String): String =>
    """
    Convert a local filesystem path to an LSP file URI.
    On Windows, normalizes backslashes to forward slashes
    and uses the `file:///` prefix required for
    drive-letter paths.
    """
    if path.contains("://") then
      path
    else
      ifdef windows then
        let s = path.clone()
        s.replace("\\", "/")
        "file:///" + consume s
      else
        "file://" + path
      end
    end
