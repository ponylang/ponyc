actor Main
  var _s: (String | None)

  new create(env: Env) =>
    var i: USize = 0
    while i < 5 do
       _s = i.string()
       i = i + 1
    else
      _s = ""
    end
