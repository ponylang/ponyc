struct \packed\ Foo
  fun foo() =>
    let bar = true

    if \likely\ bar then None end
    while \unlikely\ bar do None end
    repeat None until \likely\ bar end
    match bar | \unlikely\ bar => None end

actor Main
  new create(env: Env) =>
    None
