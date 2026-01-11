primitive Unicode is Parser
  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    let from = skip_hidden(source, offset, hidden)
    try
      (let c, let length) = source.content.utf32(from.isize())?
      if c != 0xFFFD then
        return result(source, offset, from, length.usize(), tree)
      end
    end
    (from - offset, this)

  fun error_msg(): String => "a unicode character"

type R is UnicodeRange

class UnicodeRange is Parser
  let _low: U32
  let _hi: U32

  new create(low: U32, hi: U32 = 0x10FFFF) =>
    _low = low
    _hi = hi

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    let from = skip_hidden(source, offset, hidden)
    try
      (let c, let length) = source.content.utf32(from.isize())?
      if (c != 0xFFFD) and (c >= _low) and (c <= _hi) then
        return result(source, offset, from, length.usize(), tree)
      end
    end
    (from - offset, this)

  fun error_msg(): String =>
    recover
      String
        .> append("a unicode character between ")
        .> push_utf32(_low)
        .> append(" and ")
        .> push_utf32(_hi)
    end
