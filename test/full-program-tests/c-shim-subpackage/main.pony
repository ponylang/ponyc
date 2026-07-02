use "cdefine:MAIN_VALUE=40"
use "helper"

use @main_value[I32]()

actor Main
  new create(env: Env) =>
    env.exitcode(@main_value() + Helper.value())
