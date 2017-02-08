use "net/http"

actor Main
  """
  A simple HTTP server.
  """
  new create(env: Env) =>
    let service = try env.args(1) else "50000" end
    let limit = try env.args(2).usize() else 100 end
    let host = "localhost"

    let logger = CommonLog(env.out)
    // let logger = ContentsLog(env.out)
    // let logger = DiscardLog

    let auth = try
      env.root as AmbientAuth
    else
      env.out.print("unable to use network")
      return
    end

    // Start the top server control actor.
    HTTPServer(auth,
        ListenHandler(env),
        BackendMaker.create( env ),
        logger
        where service=service, host=host, limit=limit, reversedns=auth)

class ListenHandler
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref listening(server: HTTPServer ref) =>
    try
      (let host, let service) = server.local_address().name()
    else
      _env.out.print("Couldn't get local address.")
      server.dispose()
    end

  fun ref not_listening(server: HTTPServer ref) =>
    _env.out.print("Failed to listen.")

  fun ref closed(server: HTTPServer ref) =>
    _env.out.print("Shutdown.")

class BackendMaker is HandlerFactory
  let _env: Env

  new val create( env: Env ) =>
    _env = env

  fun apply( session: HTTPSession tag ): HTTPHandler ref^ =>
    BackendHandler.create( _env, session )

class BackendHandler is HTTPHandler
  """
  Notification class for a single HTTP session.  A session can process
  several requests, one at a time.
  """
  let _env: Env
  let _session: HTTPSession tag

  new ref create( env': Env, session: HTTPSession tag ) =>
    """
    Create a context for receiving HTTP requests for a session.
    """
    _env = env'
    _session = session

  fun ref apply( request: Payload val ) =>
    """
    Start processing a request.
    """
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

    _session( consume response)
