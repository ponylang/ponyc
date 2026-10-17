primitive CommonPrefix
  """
  Creates a string that is the common prefix of the supplied strings, possibly
  empty.
  """

  fun apply(data: ReadSeq[Stringable]): String =>
    try
      let iter = data.values()
      if iter.has_next() then
        var res: String = iter.next()?.string()
        for d in iter do
          var idx: USize = 0
          let s = d.string()
          while (idx < res.size()) and (idx < s.size()) do
            if res(idx)? != s(idx)? then
              break
            end
            idx = idx + 1
          end
          res = res.substring(0, idx.isize())
        end
        res
      else
        ""
      end
    else
      ""
    end
