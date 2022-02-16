actor Main
  var _s: (String | None)

  new create(env: Env) =>
    try
      _s = "".usize()?.string()
    else
      _s = None
    end
