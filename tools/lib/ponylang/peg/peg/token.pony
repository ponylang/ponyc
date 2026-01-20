class val Token
  let _label: Label
  let source: Source
  let offset: USize
  let length: USize

  new val create(label': Label, source': Source, offset': USize, length': USize)
  =>
    _label = label'
    source = source'
    offset = offset'
    length = length'

  fun label(): Label => _label

  fun string(): String iso^ =>
    source.content.substring(offset.isize(), (offset + length).isize())

  fun substring(from: ISize, to: ISize): String iso^ =>
    source.content.substring(
      offset_to_index(from).isize(), offset_to_index(to).isize())

  fun offset_to_index(i: ISize): USize =>
    if i < 0 then offset + length + i.usize() else offset + i.usize() end
