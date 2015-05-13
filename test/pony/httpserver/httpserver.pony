use "net/http"

actor Main
  new create(env: Env) =>
    let service = try env.args(1) else "50000" end
    let limit = try env.args(2).u64() else 100 end
    Server(Info(env), Handle, CommonLog(env.out) where service = service,
      limit = limit)
    // Server(Info(env), Handle, ContentsLog(env.out) where service = service,
    //   limit = limit)

class Info
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref listening(server: Server ref) =>
    try
      (let host, let service) = server.local_address().name()
      _env.out.print("Listening on " + host + ":" + service)
    else
      _env.out.print("Couldn't get local address.")
      server.dispose()
    end

  fun ref not_listening(server: Server ref) =>
    _env.out.print("Failed to listen.")

  fun ref closed(server: Server ref) =>
    _env.out.print("Shutdown.")

primitive Handle
  fun val apply(request: Payload) =>
    let response = Payload.response()
    response.add_chunk("You asked for ")
    response.add_chunk(request.url.path)

    if request.url.query.size() > 0 then
      response.add_chunk("?")
      response.add_chunk(request.url.query)
    end

    if request.url.fragment.size() > 0 then
      response.add_chunk("#")
      response.add_chunk(request.url.fragment)
    end

    (consume request).respond(consume response)
