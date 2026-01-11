use "collections"

primitive Position
  fun apply(source: Source, offset: USize): (USize, USize) =>
    var cr = false
    var line = USize(1)
    var col = USize(1)

    try
      for i in Range(0, offset) do
        match source.content(i)?
        | '\r' =>
          line = line + 1
          col = 1
          cr = true
        | '\n' =>
          if not cr then
            line = line + 1
            col = 1
          else
            cr = false
          end
        else
          col = col + 1
          cr = false
        end
      end
    end

    (line, col)

  fun text(source: Source, offset: USize, col: USize): String =>
    let start = ((offset - col) + 1).isize()
    let finish =
      try
        source.content.find("\n", start)?
      else
        source.content.size().isize()
      end
    source.content.substring(start, finish)

  fun indent(line: String, col: USize): String =>
    recover
      let s = String
      try
        for i in Range(0, col - 1) do
          if line(i)? == '\t' then
            s.append("\t")
          else
            s.append(" ")
          end
        end
      end
      s
    end
