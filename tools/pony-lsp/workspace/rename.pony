use ".."
use "collections"
use "files"
use "json"
use "pony_compiler"

primitive Rename
  """
  Builds a WorkspaceEdit for renaming all occurrences of a symbol across all
  packages in the workspace.
  """
  fun collect(
    node: AST box,
    packages: Map[String, PackageState] box,
    workspace_folder: String,
    new_name: String): (JsonObject val | String val)
  =>
    """
    Walk all module ASTs across all packages and return a WorkspaceEdit mapping
    each file URI to the TextEdit[] needed to rename every occurrence of the
    symbol at `node` to `new_name`.

    Returns a String error message if the symbol cannot be renamed (literal,
    synthetic node, or defined outside the workspace).
    """
    let target: AST val =
      match \exhaustive\ _ResolveASTTarget(node)
      | let t: AST val => t
      | None => return "Cannot rename: symbol is not referenceable"
      end

    // Reject renames targeting symbols defined outside the workspace.
    let target_file: String val =
      try
        target.source_file() as String val
      else
        return "Cannot rename: symbol has no source location"
      end
    if not target_file.at(workspace_folder + Path.sep(), 0) then
      return "Cannot rename: symbol is defined outside the workspace"
    end

    // Collect all occurrences including the declaration.
    let locations = References.collect(node, packages, true)

    // Group TextEdit objects by file URI and build the WorkspaceEdit.
    let by_uri = Map[String, Array[LspLocation]]
    for loc in locations.values() do
      try
        by_uri(loc.uri)?.push(loc)
      else
        by_uri(loc.uri) = Array[LspLocation].create() .> push(loc)
      end
    end

    var changes = JsonObject
    for (uri, locs) in by_uri.pairs() do
      var edits = JsonArray
      for loc in locs.values() do
        edits =
          edits.push(
            JsonObject
              .update("range", loc.range.to_json())
              .update("newText", new_name))
      end
      changes = changes.update(uri, edits)
    end
    JsonObject.update("changes", changes)

primitive _IsValidPonyIdentifier
  """
  Returns true if `name` is a syntactically valid Pony identifier:
  first character must be [a-zA-Z_], subsequent characters [a-zA-Z0-9_'].
  """
  fun apply(name: String): Bool =>
    if name.size() == 0 then
      return false
    end
    var first = true
    for c in name.values() do
      if first then
        first = false
        if not (((c >= 'a') and (c <= 'z')) or
                ((c >= 'A') and (c <= 'Z')) or
                (c == '_'))
        then
          return false
        end
      else
        if not (((c >= 'a') and (c <= 'z')) or
                ((c >= 'A') and (c <= 'Z')) or
                ((c >= '0') and (c <= '9')) or
                (c == '_') or (c == '\''))
        then
          return false
        end
      end
    end
    true
