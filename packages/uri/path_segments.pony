primitive PathSegments
  """
  Split a URI path on `/` delimiters, then percent-decode each segment.

  A leading `/` produces an empty first segment (distinguishes absolute
  from relative paths). Splitting happens before decoding, so a
  percent-encoded `/` (`%2F`) within a segment is preserved as a literal
  slash in the decoded segment, not treated as a delimiter.
  """
  fun apply(path: String val)
    : (Array[String val] val | InvalidPercentEncoding val)
  =>
    """
    Split `path` on `/` and percent-decode each segment.
    """
    // Empty path produces a single empty segment
    if path.size() == 0 then
      return recover val Array[String val](1) .> push("") end
    end

    // Count segments for pre-allocation
    var count: USize = 1
    for c in path.values() do
      if c == '/' then count = count + 1 end
    end

    let segments = Array[String val](count)
    var start: USize = 0
    var i: USize = 0

    while i < path.size() do
      try
        if path(i)? == '/' then
          let raw: String val = path.substring(start.isize(), i.isize())
          match \exhaustive\ PercentDecode(raw)
          | let decoded: String val => segments.push(decoded)
          | let err: InvalidPercentEncoding val => return err
          end
          start = i + 1
        end
      else
        _Unreachable()
      end
      i = i + 1
    end

    // Final segment after last '/' (or entire path if no '/')
    let raw: String val = path.substring(start.isize(), path.size().isize())
    match \exhaustive\ PercentDecode(raw)
    | let decoded: String val => segments.push(decoded)
    | let err: InvalidPercentEncoding val => return err
    end

    let result: Array[String val] iso =
      recover iso
        Array[String val](segments.size())
      end
    for seg in segments.values() do
      result.push(seg)
    end
    consume result
