actor Main
  var _s: (String | None)

  new create(env: Env) =>
    try
      "".usize()?.string()
    then
      _s = "initialized"
    end
