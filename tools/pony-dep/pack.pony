use "collections"
use "files"

primitive PackSourceNotFound
  """
  The source path does not exist or cannot be stat'd.
  """

primitive PackSourceNotDirectory
  """
  The source path exists but is not a directory.
  """

primitive PackOutputInsideSource
  """
  The output path falls inside the source directory.
  """

primitive PackFailed
  """
  An internal error occurred during packing.
  """

type PackError is
  ( PackSourceNotFound
  | PackSourceNotDirectory
  | PackOutputInsideSource
  | PackFailed )

primitive Pack
  """
  Creates a `.par` archive from a source directory. Returns the content hash
  when requested. Walks the source tree once, feeding each file to both the
  archive encoder and the content hasher.
  """
  fun apply(
    auth: FileAuth,
    source_path: String,
    output_path: String,
    hash: Bool)
    : (String val | None | PackError)
  =>
    """
    Packs the directory at `source_path` into a `.par` archive at
    `output_path`. Returns the hex-encoded content hash when `hash` is true.
    """
    let source =
      try
        let s = FilePath(auth, source_path)
        let info = FileInfo(s)?
        if not info.directory then
          return PackSourceNotDirectory
        end
        s
      else
        return PackSourceNotFound
      end

    let abs_source = Path.abs(source_path)
    let abs_out = Path.abs(output_path)
    let prefix: String val = abs_source + Path.sep()
    if abs_out.at(prefix) or (abs_out == abs_source) then
      return PackOutputInsideSource
    end

    try
      let encoder = ArchiveEncoder(source)?
      let leaves: (Array[_LeafEntry] | None) =
        if hash then Array[_LeafEntry] else None end

      _walk(source, source.path, encoder, leaves)?

      let output = FilePath(auth, output_path)
      encoder.write(output)?

      match leaves
      | let l: Array[_LeafEntry] =>
        let root_hash = ContentHash._from_leaves(l)
        let s: String val = "sha256:" + Sha256.hex(root_hash)
        s
      else
        None
      end
    else
      PackFailed
    end

  fun _walk(
    dir: FilePath,
    root_path: String,
    encoder: ArchiveEncoder ref,
    leaves: (Array[_LeafEntry] | None)) ?
  =>
    let name = _relative_path(root_path, dir.path)
    encoder.add_dir_entry(name)?

    with d = Directory(dir)? do
      let sorted = Sort[Array[String], String](d.entries()?)
      for entry_name in sorted.values() do
        let child = dir.join(entry_name)?
        let child_info = FileInfo(child)?
        if child_info.symlink then
          None
        elseif child_info.file then
          let rel = _relative_path(root_path, child.path)
          let content: Array[U8] val =
            match OpenFile(child)
            | let f: File =>
              let data = f.read(f.size())
              f.dispose()
              consume data
            else
              error
            end
          encoder.add_file_entry(rel, content)?
          match leaves
          | let l: Array[_LeafEntry] =>
            l.push(_LeafEntry(rel, ContentHash.leaf_hash(rel, content)))
          end
        elseif child_info.directory then
          _walk(child, root_path, encoder, leaves)?
        end
      end
    end

  fun _relative_path(root_path: String, path: String): String =>
    let rel =
      try
        Path.rel(root_path, path)?
      else
        _Unreachable()
        ""
      end
    ifdef windows then
      rel.clone() .> replace("\\", "/")
    else
      rel
    end
