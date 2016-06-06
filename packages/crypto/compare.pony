use "collections"

primitive ConstantTimeCompare
  fun apply(xs: ByteSeq, ys: ByteSeq): Bool =>
  """
  Return true if the two ByteSeqs, xs and ys, have equal contents. The time
  taken is independent of the contents.
  """
  if xs.size() != ys.size() then
    false
  else
    var v = U8(0)
    for i in Range[USize](0, xs.size()) do
      try
        v = v or (xs(i) xor ys(i))
      else
        return false
      end
    end
    v == 0
  end
