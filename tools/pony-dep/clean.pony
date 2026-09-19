use "collections"
use "files"
use ast = "pony_compiler"

primitive CleanSourceNotFound
  """
  The source directory does not exist.
  """

primitive CleanSourceNotDirectory
  """
  The source path exists but is not a directory.
  """

primitive CleanExtNotFound
  """
  The ext directory does not exist.
  """

primitive CleanExtNotDirectory
  """
  The ext path exists but is not a directory.
  """

class val CleanFailed
  """
  An error prevented the clean from completing — the source directory
  could not be read or parsed.
  """

  let message: String val

  new val create(message': String val) =>
    message = message'

primitive Clean
  """
  Removes directories from `ext` that no code references. Liveness is
  determined by `use "ext:..."` statements in the parsed AST — the deps
  file is not consulted. A directory placed manually survives if
  referenced and is cleaned if not.

  Reachability is transitive: if src references A and A references B,
  both are live.
  """
  fun apply(
    auth: FileAuth,
    src_dir: String,
    ext_dir: String,
    package_paths: (String box | ReadSeq[String val] box))
    : (Array[String val] val | CleanSourceNotFound
      | CleanSourceNotDirectory | CleanExtNotFound
      | CleanExtNotDirectory | CleanFailed)
  =>
    """
    Returns the names of directories removed from `ext_dir`, or an error.
    """
    let src_path = FilePath(auth, src_dir)
    let ext_path = FilePath(auth, ext_dir)

    let src_info =
      try FileInfo(src_path)?
      else return CleanSourceNotFound
      end
    if not src_info.directory then
      return CleanSourceNotDirectory
    end

    let ext_info =
      try FileInfo(ext_path)?
      else return CleanExtNotFound
      end
    if not ext_info.directory then
      return CleanExtNotDirectory
    end

    let reachable = HashSet[String, HashEq[String]]
    let queue = Array[String val]

    let src_refs =
      match \exhaustive\ _collect_ext_refs(src_path, package_paths)
      | let refs: HashSet[String, HashEq[String]] => refs
      | None =>
        return CleanFailed(
          "cannot parse source directory: " + src_dir)
      end
    for ref_name in src_refs.values() do
      if not reachable.contains(ref_name) then
        reachable.set(ref_name)
        queue.push(ref_name)
      end
    end

    while queue.size() > 0 do
      try
        let name = queue.shift()?
        let pkg_path = FilePath(auth, Path.join(ext_dir, name))
        if
          try FileInfo(pkg_path)?.directory else false end
        then
          match _collect_ext_refs(pkg_path, package_paths)
          | let pkg_refs: HashSet[String, HashEq[String]] =>
            for ref_name in pkg_refs.values() do
              if not reachable.contains(ref_name) then
                reachable.set(ref_name)
                queue.push(ref_name)
              end
            end
          end
        end
      else
        _Unreachable()
      end
    end

    let to_remove = Array[String val]
    try
      with d = Directory(ext_path)? do
        let entries = d.entries()?
        for entry in (consume entries).values() do
          try
            let entry_path = ext_path.join(entry)?
            if
              (try FileInfo(entry_path)?.directory else false end) and
                not reachable.contains(entry)
            then
              to_remove.push(entry)
            end
          end
        end
      end
    else
      return CleanFailed("cannot read ext directory: " + ext_dir)
    end

    let removed = recover iso Array[String val] end
    try
      with d = Directory(ext_path)? do
        for name in to_remove.values() do
          if not d.remove(name) then
            return CleanFailed("cannot remove directory: " + name)
          end
          removed.push(name)
        end
      end
    else
      return CleanFailed("cannot read ext directory: " + ext_dir)
    end
    consume removed

  fun _collect_ext_refs(
    dir: FilePath,
    package_paths: (String box | ReadSeq[String val] box))
    : (HashSet[String, HashEq[String]] | None)
  =>
    """
    Parses the directory at PassParse and extracts all `use "ext:..."`
    identifiers from the AST. Returns None when the directory fails to parse.
    """
    match \exhaustive\ ast.Compiler.compile(
      dir where package_search_paths = package_paths,
      limit = ast.PassParse)
    | let program: ast.Program val =>
      let refs = HashSet[String, HashEq[String]]
      match program.package()
      | let pkg: ast.Package val =>
        for mod in pkg.modules() do
          let collector = _ExtRefCollector(refs)
          mod.ast.visit(collector)
        end
      end
      refs
    | let _: Array[ast.Error] val =>
      None
    end

class ref _ExtRefCollector is ast.ASTVisitor
  """
  AST visitor that collects identifiers from `use "ext:..."` statements.
  The identifier is the part after `ext:` — the directory name in `ext`.
  """
  let _refs: HashSet[String, HashEq[String]]

  new ref create(refs: HashSet[String, HashEq[String]]) =>
    _refs = refs

  fun ref visit(node: ast.AST box): ast.VisitResult =>
    if node.id() == ast.TokenIds.tk_use() then
      _try_collect_use(node)
    end
    ast.Continue

  fun ref _try_collect_use(node: ast.AST box) =>
    try
      let uri_node = node(1)?
      if uri_node.id() == ast.TokenIds.tk_string() then
        _try_add_ext_ref(uri_node)
      end
    end

  fun ref _try_add_ext_ref(uri_node: ast.AST box) =>
    match uri_node.token_value()
    | let uri: String val if uri.at("ext:") =>
      _refs.set(uri.substring(4))
    end
