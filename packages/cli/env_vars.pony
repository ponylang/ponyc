use "collections"

primitive EnvVars
  fun apply(
    envs: (Array[String] box | None),
    prefix: String = "",
    squash: Bool = false):
    Map[String, String] val
  =>
    """
    Turns an array of strings that look like environment variables, ie.
    key=value, into a map from string to string. Can optionally filter for
    keys matching a 'prefix', and will squash resulting keys to lowercase
    iff 'squash' is true.

    So:
      <PREFIX><KEY>=<VALUE>
    becomes:
      {KEY, VALUE} or {key, VALUE}
    """
    let envsmap = recover Map[String, String]() end
    match envs
    | let envsarr: Array[String] box =>
      let prelen = prefix.size().isize()
      for e in envsarr.values() do
        let eqpos = try e.find("=") else ISize.max_value() end
        let ek: String val = e.substring(0, eqpos)
        let ev: String val = e.substring(eqpos + 1)
        if (prelen == 0) or ek.at(prefix, 0) then
          if squash then
            envsmap.update(ek.substring(prelen).lower(), ev)
          else
            envsmap.update(ek.substring(prelen), ev)
          end
        end
      end
    end
    envsmap
