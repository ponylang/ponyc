use "./pkg"

actor Main
  new create(env: Env) =>
    None

class C is T
  fun _override(): (T | None) =>
    None

  fun _get_opc(): OPC =>
    OPC
