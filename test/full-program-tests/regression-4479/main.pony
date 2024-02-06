actor Main
  new create(env: Env) => None
    let cb: Callbacks = Callbacks

class Callbacks
  var cbi: @{(): None}

  new create() =>
    cbi = @{() => None}
