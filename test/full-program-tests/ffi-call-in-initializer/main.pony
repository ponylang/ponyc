use "lib:ffi-call-in-initializer-additional"

use @ffi_function[I32]()

// From issue #4086
actor Main
  let _let_field: I32 = @ffi_function()
  var _var_field: I32 = @ffi_function()

  new create(env: Env) =>
    None
