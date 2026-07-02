class _Lambda
  """
  Demonstrates hover on lambda types.
  """

  let _callback: {(String val): None val} val

  new create(callback: {(String val): None val} val) =>
    _callback = callback

  fun demo_lambda_local_vars(): String =>
    let f = {(): String => "hello" }
    let add = {(a: U32, b: U32): U32 => a + b }
    f() + add(1, 2).string()

  fun demo_lambda_param(callback: {(U32 val): String val} val): String =>
    callback(42)
