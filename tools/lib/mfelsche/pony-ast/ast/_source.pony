use @source_open[NullablePointer[_Source]](file: Pointer[U8] tag, error_msgp: NullablePointer[Pointer[U8]])
use @source_open_string[NullablePointer[_Source]](source_code: Pointer[U8] tag)
use @source_close[None](source: _Source)

struct _Source
  let file: Pointer[U8] val = file.create()
    """
    Absolute path to the file of this source.
    """
  let m: Pointer[U8] ref = m.create()
    """
    zero terminated string holding the complete source.
    """
  let len: USize = 0
    """
    Length of the source, including the `'\0'` terminator.
    """
