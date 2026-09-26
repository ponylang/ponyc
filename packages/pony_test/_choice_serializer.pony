primitive _ChoiceSerializer
  """
  Serializes and deserializes choice sequences to and from text.
  """
  fun serialize(choices: Array[_Choice val] val): String iso^ =>
    """
    Produce a text representation of a choice sequence.

    Format: one line per choice, tagged by type. First line is a version
    header.
    """
    let buf = recover iso String end
    buf.append("ponycheck v1\n")
    for choice in choices.values() do
      match \exhaustive\ choice
      | let c: _IntChoice =>
        buf
          .> append("I ")
          .> append(c.value.string())
          .> append(" ")
          .> append(c.min.string())
          .> append(" ")
          .> append(c.max.string())
          .> append(" ")
          .> append(c.shrink_towards.string())
          .> append("\n")
      | let c: _U128Choice =>
        buf
          .> append("U ")
          .> append(c.value.string())
          .> append(" ")
          .> append(c.min.string())
          .> append(" ")
          .> append(c.max.string())
          .> append(" ")
          .> append(c.shrink_towards.string())
          .> append("\n")
      | let c: _FloatChoice =>
        buf
          .> append("F ")
          .> append(c.value.bits().string())
          .> append(" ")
          .> append(c.min.bits().string())
          .> append(" ")
          .> append(c.max.bits().string())
          .> append("\n")
      | let c: _BoolChoice =>
        buf
          .> append("B ")
          .> append(c.value.string())
          .> append(" ")
          .> append(c.forced.string())
          .> append("\n")
      end
    end
    consume buf

  fun deserialize(data: String val): (Array[_Choice val] val | None) =>
    """
    Parse a serialized choice sequence. Returns None on any parse failure.
    """
    let lines = data.split_by("\n")
    if lines.size() < 1 then return None end

    try
      let header = lines(0)?
      if header != "ponycheck v1" then return None end
    else
      return None
    end

    let choices = recover iso Array[_Choice val] end
    var i: USize = 1
    while i < lines.size() do
      let line =
        try lines(i)?
        else return None
        end
      i = i + 1

      if line.size() == 0 then continue end

      let parts = line.split_by(" ")
      try
        let kind = parts(0)?
        if kind == "I" then
          if parts.size() != 5 then return None end
          choices.push(_IntChoice(
            parts(1)?.i128()?,
            parts(2)?.i128()?,
            parts(3)?.i128()?,
            parts(4)?.i128()?))
        elseif kind == "U" then
          if parts.size() != 5 then return None end
          choices.push(_U128Choice(
            parts(1)?.u128()?,
            parts(2)?.u128()?,
            parts(3)?.u128()?,
            parts(4)?.u128()?))
        elseif kind == "F" then
          if parts.size() != 4 then return None end
          choices.push(_FloatChoice(
            F64.from_bits(parts(1)?.u64()?),
            F64.from_bits(parts(2)?.u64()?),
            F64.from_bits(parts(3)?.u64()?)))
        elseif kind == "B" then
          if parts.size() != 3 then return None end
          let v = parts(1)?
          let f = parts(2)?
          let bval =
            if v == "true" then true
            elseif v == "false" then false
            else return None
            end
          let forced =
            if f == "true" then true
            elseif f == "false" then false
            else return None
            end
          choices.push(_BoolChoice(bval, forced))
        else
          return None
        end
      else
        return None
      end
    end
    consume choices
