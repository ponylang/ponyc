class _LambdaHints
  """
  Lambda type annotation and inferred lambda local variable hints.
  """

  fun box demo_inferred(): None val =>
    let f = {(): String => "hello" }
    let add = {(a: U32, b: U32): U32 => a + b }
    f() + add(1, 2).string()

  fun box demo_annotated(): None val =>
    let g: {(String): None} val = {(s: String) => None }
    None

  fun box demo_annotated_explicit(): None val =>
    let h: {(String val): None val} val = {(s: String) => None }
    None

  fun box with_callback(callback: {(String): None} val): None val =>
    None

  fun box with_callback_explicit(
    callback: {(String val): None val} val): None val =>
    None

  fun box make_cb(): {(): String} val =>
    {(): String => "hello" }

  fun box make_cb_explicit(): {(): String val} val =>
    {(): String => "hello" }

  fun box with_generic_param(
    callback: {(Array[U32]): None} val): None val =>
    None

  fun box with_nested_lambda(
    callback: {({(): String}): None} val): None val =>
    None

  fun box with_union_param(
    callback: {((String | None)): None} val): None val =>
    None
