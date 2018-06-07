use "net/http"

actor Main
  """
  A simple HTTP server.
  """
  new create(env: Env) =>
    let service = try env.args(1)? else "50000" end
    let limit = try env.args(2)?.usize()? else 100 end
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
    HTTPServer(
      auth,
      ListenHandler(env),
      BackendMaker.create(env),
      logger
      where service=service, host=host, limit=limit, reversedns=auth)

class ListenHandler
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref listening(server: HTTPServer ref) =>
    try
      (let host, let service) = server.local_address().name()?
      _env.out.print("connected: " + host)
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

  new val create(env: Env) =>
    _env = env

  fun apply(session: HTTPSession): HTTPHandler^ =>
    BackendHandler.create(_env, session)

class BackendHandler is HTTPHandler
  """
  Notification class for a single HTTP session.  A session can process
  several requests, one at a time.  Data recieved using OneshotTransfer 
  transfer mode is echoed in the response.
  """
  let _env: Env
  let _session: HTTPSession
  var _response: Payload = Payload.response()

  new ref create(env: Env, session: HTTPSession) =>
    """
    Create a context for receiving HTTP requests for a session.
    """
    _env = env
    _session = session

  fun ref apply(request: Payload val) =>
    """
    Start processing a request.
    """
    _response.add_chunk("You asked for ")
    _response.add_chunk(request.url.path)

    if request.url.query.size() > 0 then
      _response.add_chunk("?")
      _response.add_chunk(request.url.query)
    end

    if request.url.fragment.size() > 0 then
      _response.add_chunk("#")
      _response.add_chunk(request.url.fragment)
    end

    if request.method.eq("GET") then
      _session(_response = Payload.response())
    end

  fun ref chunk(data: ByteSeq val) =>
    """
    Process the next chunk of data received.
    """
    _response.add_chunk("\n")
    _response.add_chunk(data)

  fun ref finished() =>
    """
    Called when the last chunk has been handled.
    """
    _env.out.print("Finished")
    _session(_response = Payload.response())
