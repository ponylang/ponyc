use @errors_alloc[NullablePointer[_Errors]]()
use @errors_free[None](errors: _Errors)
use @errors_print[None](errors: _Errors)
use @errors_get_first[NullablePointer[_ErrorMsg]](errors: _Errors)
use @errors_get_count[USize](errors: _Errors)

struct _TypecheckFrame
  var package: Pointer[_AST] val = package.create()
  var module: Pointer[_AST] val = module.create()
  var ttype: Pointer[_AST] val = ttype.create()
  var constraint: Pointer[_AST] val = constraint.create()
  var provides: Pointer[_AST] val = provides.create()
  var method: Pointer[_AST] val = method.create()
  var def_arg: Pointer[_AST] val = def_arg.create()
  var method_body: Pointer[_AST] val = method_body.create()
  var method_params: Pointer[_AST] val = method_params.create()
  var method_type: Pointer[_AST] val = method_type.create()
  var ffi_type: Pointer[_AST] val = ffi_type.create()
  var as_type: Pointer[_AST] val = as_type.create()
  var the_case: Pointer[_AST] val = the_case.create()
  var pattern: Pointer[_AST] val = pattern.create()
  var loop: Pointer[_AST] val = loop.create()
  var loop_cond: Pointer[_AST] val = loop_cond.create()
  var loop_body: Pointer[_AST] val = loop_body.create()
  var loop_else: Pointer[_AST] val = loop_else.create()
  var try_expr: Pointer[_AST] val = try_expr.create()
  var rrecover: Pointer[_AST] val = rrecover.create()
  var ifdef_cond: Pointer[_AST] val = ifdef_cond.create()
  var ifdef_clause: Pointer[_AST] val = ifdef_clause.create()
  var iftype_constraint: Pointer[_AST] val = iftype_constraint.create()
  var iftype_body: Pointer[_AST] val = iftype_body.create()

  var prev: NullablePointer[_TypecheckFrame] ref = prev.none()


struct _TypecheckStats
  var names_count: USize = 0
  var default_caps_count: USize = 0
  var heap_alloc: USize = 0
  var stack_alloc: USize = 0


class val Error
  let file: (String val | None)
    """
    Absolute path to the file containing this error.

    Is `None` for errors without file context.
    """
  let position: Position
  let msg: String
    """
    Error Message.
    """
  let infos: Array[Error] val
    """
    Additional informational messages, possibly with a file context.
    """
  let source_snippet: (String val | None)
    """
    Used for displaying the error message in the context of the source.

    Is `None` for errors without file context.
    """

  new val create(msg': _ErrorMsg val) =>
    """
    Copy out all the error information, so the ErrorMsg can be deleted afterwards
    """
    let file_ptr = msg'.file
    file =
      if file_ptr.is_null() then
        None
      else
        recover val String.copy_cstring(file_ptr) end
      end
    position = Position(msg'.line, msg'.pos)
    let msg_ptr = msg'.msg
    msg = recover val String.copy_cstring(msg_ptr) end
    let src_ptr = msg'.source
    source_snippet =
      if src_ptr.is_null() then
        None
      else
        recover val String.copy_cstring(src_ptr) end
      end

    var frame_ptr = msg'.frame
    let infos_arr = recover trn Array[Error].create(0) end
    while not frame_ptr.is_none() do
      try
        let frame: _ErrorMsg val = frame_ptr()?
        infos_arr.push(Error.create(frame))
        frame_ptr = frame.frame
      end
    end
    infos = consume infos_arr

  new val message(message': String val) =>
    """
    Create an error with file context, only containing the given `message`.
    """
    file = None
    position = Position.min()
    msg = message'
    source_snippet = None
    infos = Array[Error].create(0)

  fun has_file_context(): Bool =>
    (file isnt None) and (position > Position.min())


struct val _ErrorMsg
  var file: Pointer[U8] val = recover val file.create() end
  var line: USize = 0
  var pos: USize = 0
  var msg: Pointer[U8] val = recover val msg.create() end
  var source: Pointer[U8] val = recover val source.create() end
  var frame: NullablePointer[_ErrorMsg] val = frame.none()
  var next: NullablePointer[_ErrorMsg] val = next.none()

struct _FILE
  """STUB"""

struct _Errors
  var head: NullablePointer[_ErrorMsg] val = head.none()
  var tail: NullablePointer[_ErrorMsg] val = tail.none()
  var count: USize = 0
  var immediate_report: Bool = false
  var output_stream: Pointer[I32] = output_stream.create() // FILE*

  fun ref extract(): Array[Error] val =>
    let errs = recover trn Array[Error].create(this.count) end
    var msg_ptr: NullablePointer[_ErrorMsg] val = head
    while not msg_ptr.is_none() do
      try
        let msg: _ErrorMsg val = msg_ptr()?
        errs.push(Error.create(msg))
        msg_ptr = msg.next
      end
    end
    consume errs

struct _Typecheck
  """
  """
  var frame: NullablePointer[_TypecheckFrame] ref
  embed stats: _TypecheckStats
  var errors: NullablePointer[_Errors]

  new create() =>
    """
    no allocation
    """
    frame = NullablePointer[_TypecheckFrame].none()
    stats = _TypecheckStats.create()
    errors = NullablePointer[_Errors].none()

