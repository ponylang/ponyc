actor Main
  new create(env: Env) =>
    try foo(true) end

  fun ref foo(a: Bool): U64 ? =>
    // works
    // match a
    // | true => return 1
    // else
    //   error
    // end

    // works
    // if a then
    //   return 1
    // else
    //   error
    // end

    // works
    //while a do
    //  return 1
    //else
    //  error
    //end

    // works
    // repeat
    //   return 1
    // until
    //   a
    // else
    //   error
    // end

    // works
    // try
    //   if a then
    //     return 1
    //   else
    //     error
    //   end
    // else
    //   error
    // end

    error
