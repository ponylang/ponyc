use "collections"
use "files"

class _LeafEntry is Comparable[_LeafEntry]
  let path: String val
  let hash: Array[U8] val

  new create(path': String val, hash': Array[U8] val) =>
    path = path'
    hash = hash'

  fun eq(that: _LeafEntry box): Bool => path == that.path
  fun lt(that: _LeafEntry box): Bool => path < that.path

primitive ContentHash
  """
  Computes a Merkle root hash over files in a directory tree. Walks the
  directory recursively, hashing each file's content on the spot and
  keeping only the 32-byte leaf hash — file content is not held in
  memory across files.

  Leaf hashes are sorted by relative path as raw bytes (lexicographic).
  Each leaf is SHA-256(path_bytes || 0x00 || content_bytes), binding
  the path into the hash to prevent swapped-content collisions. Interior
  nodes are SHA-256(left || right). When a level has an odd number of
  nodes, the last node promotes to the next level without pairing.

  Symlinks are skipped. Errors if a file cannot be read.

  Returns 32 zero bytes for an empty directory tree.
  """
  fun apply(root: FilePath): Array[U8] val ? =>
    """
    Returns the 32-byte Merkle root hash of all files under `root`.
    """
    let leaves = Array[_LeafEntry]
    _walk(root, root.path, leaves)?
    Sort[Array[_LeafEntry], _LeafEntry](leaves)

    if leaves.size() == 0 then
      return recover val Array[U8].init(U8(0), 32) end
    end

    var level = Array[Array[U8] val](leaves.size())
    for entry in leaves.values() do
      level.push(entry.hash)
    end

    while level.size() > 1 do
      let next = Array[Array[U8] val]((level.size() + 1) / 2)
      var j: USize = 0
      while (j + 1) < level.size() do
        try
          let left = level(j)?
          let right = level(j + 1)?
          let pair =
            recover val
              Array[U8](64)
                .> append(left)
                .> append(right)
            end
          next.push(Sha256(pair))
        else
          _Unreachable()
        end
        j = j + 2
      end
      if (level.size() % 2) == 1 then
        try next.push(level(level.size() - 1)?) else _Unreachable() end
      end
      level = next
    end

    try level(0)? else _Unreachable(); recover val Array[U8] end end

  fun _walk(
    dir: FilePath,
    root_path: String,
    leaves: Array[_LeafEntry])
    ?
  =>
    with d = Directory(dir)? do
      let sorted = Sort[Array[String], String](d.entries()?)
      for entry_name in sorted.values() do
        let child = dir.join(entry_name)?
        let info = FileInfo(child)?
        if info.symlink then
          None
        elseif info.file then
          let rel =
            try
              Path.rel(root_path, child.path)?
            else
              _Unreachable()
              ""
            end
          let rel_path: String val =
            ifdef windows then
              rel.clone() .> replace("\\", "/")
            else
              rel
            end
          let content: Array[U8] val =
            match OpenFile(child)
            | let f: File =>
              let data = f.read(f.size())
              f.dispose()
              consume data
            else
              error
            end
          let leaf_input =
            recover val
              Array[U8](rel_path.size() + 1 + content.size())
                .> append(rel_path)
                .> push(0x00)
                .> append(content)
            end
          leaves.push(_LeafEntry(rel_path, Sha256(leaf_input)))
        elseif info.directory then
          _walk(child, root_path, leaves)?
        end
      end
    end
