use "files"

// pony-lint: allow style/acronym-casing
primitive Uris
  """
  URI conversion utilities for the language server.
  """

  fun to_path(uri: String): String =>
    """
    Convert an LSP document URI to a local filesystem path by stripping the
    scheme and percent-decoding what is left. On Windows, also strips the
    leading `/` before a drive letter (e.g., `/D:/path` becomes `D:/path`).

    A string with no `://` is taken to be a path already and is returned
    without decoding: a `%` in a path is part of the filename. On Windows,
    backslashes are normalized to `/`.
    """
    let result = uri.split_by("://", 2)
    let path =
      if result.size() > 1 then
        try
          PercentEncoding.decode(result(1)?)
        else
          // split_by returned more than one part, so index 1 exists.
          _Unreachable()
          uri
        end
      else
        ifdef windows then
          Path.clean(uri)
        else
          uri
        end
      end
    ifdef windows then
      // file:///D:/path and file:///D%3A/path both reach here as /D:/path,
      // the scheme stripped and the path decoded; remove the leading /
      try
        if (path.size() >= 3) and (path(0)? == '/') and (path(2)? == ':') then
          path.substring(1)
        else
          path
        end
      else
        path
      end
    else
      path
    end

  fun from_path(path: String): String =>
    """
    Convert a local filesystem path to an LSP file URI, percent-encoding the
    path. On Windows, uses the `file:///` prefix required for drive-letter
    paths.

    A string that is already a URI is returned as it is, so that its escapes
    are not encoded a second time.
    """
    if path.contains("://") then
      path
    else
      let prefix = ifdef windows then "file:///" else "file://" end
      prefix + PercentEncoding.encode(path)
    end
