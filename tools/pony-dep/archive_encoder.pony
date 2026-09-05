use "buffered"
use "collections"
use "files"

class ArchiveEncoder
  """
  Encodes files and directories into a Pony archive (.par).

  All paths are stored relative to the root directory passed at
  construction. Directory contents are sorted lexicographically at each
  level for deterministic output. Symlinks are skipped. Errors on
  unreadable files.
  """
  let _root: FilePath
  embed _writer: Writer = Writer

  new create(root: FilePath) ? =>
    """
    Errors if `root` lacks FileStat capability.
    """
    if not root.caps(FileStat) then
      error
    end
    _root = root
    _writer.u8(1)

  fun ref add(from: FilePath) ? =>
    """
    Descends recursively into directories. Symlinks are skipped. Errors on
    unreadable files.
    """
    if not from.caps(FileStat) then
      error
    end

    let info = FileInfo(from)?
    if info.symlink then
      return
    end

    if info.file then
      _add_file(from)?
    elseif info.directory then
      _add_directory(from)?
    end

  fun ref write(to: FilePath) ? =>
    """
    Replaces any existing file at `to`. Errors if the file cannot be
    created. The encoder must not be reused after calling this.
    """
    match CreateFile(to)
    | let archive: File =>
      if not archive.set_length(0) then
        archive.dispose()
        error
      end
      for chunk in _writer.done().values() do
        if not archive.write(chunk) then
          archive.dispose()
          error
        end
      end
      archive.dispose()
    else
      error
    end

  fun ref _add_file(entry: FilePath) ? =>
    let name = _relative_path(entry.path)
    if name.size() > U32.max_value().usize() then error end

    let content: Array[U8] val =
      match OpenFile(entry)
      | let file: File =>
        let data = file.read(file.size())
        file.dispose()
        consume data
      else
        error
      end

    if content.size() > U32.max_value().usize() then error end
    _writer.u8(1)
    _writer.u32_le(name.size().u32())
    _writer.write(name)
    _writer.u32_le(content.size().u32())
    _writer.write(content)

  fun ref _add_directory(dir: FilePath) ? =>
    let name = _relative_path(dir.path)
    if (name.size() > 0) and (name != ".") then
      if name.size() > U32.max_value().usize() then error end
      _writer.u8(2)
      _writer.u32_le(name.size().u32())
      _writer.write(name)
    end

    with d = Directory(dir)? do
      let sorted = Sort[Array[String], String](d.entries()?)
      for entry_name in sorted.values() do
        let child = dir.join(entry_name)?
        let info = FileInfo(child)?
        if info.symlink then
          None
        elseif info.file then
          _add_file(child)?
        elseif info.directory then
          _add_directory(child)?
        end
      end
    end

  fun _relative_path(path: String): String =>
    let rel =
      try
        Path.rel(_root.path, path)?
      else
        _Unreachable()
        ""
      end
    ifdef windows then
      rel.clone().>replace("\\", "/")
    else
      rel
    end
