use "buffered"
use "collections"
use "files"

primitive CoverageError
  """
  A parse or I/O failure when loading the coverage file.
  """

primitive CoverageOk
  """
  The coverage file loaded successfully.
  """

type CoverageResult is
  ( (Array[U64] val, CoverageOk)
  | (String val, CoverageError) )

primitive CoverageFile
  """
  Reads a binary `.coverage` file produced by a `--mutate` build's baseline
  run. The file is little-endian: a 4-byte int32 count followed by count
  8-byte int64 counters, one per coverage slot.
  """

  fun read(path: String val, auth: FileAuth): CoverageResult =>
    """
    Returns an array of per-slot hit counts, or an error string on failure.
    """
    let fp = FilePath(auth, path)
    let data: Array[U8] val =
      match OpenFile(fp)
      | let file: File => file.read(file.size())
      else
        return ("cannot open " + path, CoverageError)
      end

    if data.size() < 4 then
      return ("coverage file too small: " + path, CoverageError)
    end

    let reader = Reader
    reader.append(data)

    let count: I32 =
      try reader.i32_le()?
      else return ("cannot read count from " + path, CoverageError)
      end

    if count < 0 then
      return ("negative count in " + path, CoverageError)
    end

    let expected: USize = 4 + (count.usize() * 8)
    if data.size() < expected then
      return ("truncated coverage file: " + path, CoverageError)
    end

    let counters = recover iso Array[U64] end
    try
      for _ in Range[USize].till(0, count.usize()) do
        counters.push(reader.u64_le()?)
      end
    else
      return ("cannot read counters from " + path, CoverageError)
    end

    (consume counters, CoverageOk)
