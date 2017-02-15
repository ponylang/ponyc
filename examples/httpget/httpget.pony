use "assert"
use "collections"
use "net/http"
use "net/ssl"
use "files"
use "encode/base64"
use "options"

actor Main
  """
  Fetch data from URLs on the command line.
  """
  var user: String = ""
  var pass: String = ""
  var output: String = ""

  new create(env: Env) =>
    // Get common command line options.
    let opts = Options(env.args)
    opts
      .add("user", "u", StringArgument)
      .add("pass", "p", StringArgument)
      .add("output", "o", StringArgument)

    for option in opts do
      match option
      | ("user", let u: String) => user = u
      | ("pass", let p: String) => pass = p
      | ("output", let o: String) => output = o
      | let err: ParseError =>
         err.report(env.out)
         _usage(env.out)
         return
      end
    end

    // The positional parameter is the URL to be fetched.
    let urlref = try env.args(env.args.size() - 1) else "" end
    let url = try
      URL.valid(urlref)
    else
      env.out.print("Invalid URL")
      return
    end

    // Start the actor that does the real work.
    _GetWork.create(env, url, user, pass, output)

  fun tag _usage(out: StdStream) =>
    """
    Print command error message.
    """
    out.print(
      "httpget [OPTIONS] URL\n" +
      "  --user (-u)  Username for authenticated queries\n" +
      "  --pass (-p)  Password for authenticated queries\n" +
      "  --output (-o) Name of file to write response body\n")

actor _GetWork
  """
  Do the work of fetching a resource
  """
  let _env: Env

  new create(env: Env, url: URL, user: String, pass: String, output: String)
    =>
    """
    Create the worker actor.
    """
    _env = env

    // Get certificate for HTTPS links.
    let sslctx = try
      recover
        SSLContext
          .>set_client_verify(true)
          .>set_authority(FilePath(env.root as AmbientAuth, "cacert.pem"))
        end
      end

    try
      // The Client manages all links.
      let client = HTTPClient(env.root as AmbientAuth, consume sslctx)
      // The Notify Factory will create HTTPHandlers as required.  It is
      // done this way because we do not know exactly when an HTTPSession
      // is created - they can be re-used.
      let dumpMaker = recover val NotifyFactory.create(this) end

      try
        // Start building a GET request.
        let req = Payload.request("GET", url)
        req("User-Agent") = "Pony httpget"

        // Add authentication if supplied.  We use the "Basic" format,
        // which is username:password in base64.  In a real example,
	      // you would only use this on an https link.
        if user.size() > 0 then
          let keyword = "Basic "
          let content = recover String(user.size() + pass.size() + 1) end
          content.append(user)
          content.append(":")
          content.append(pass)
          let coded = Base64.encode(consume content)
          let auth = recover String(keyword.size() + coded.size()) end
          auth.append(keyword)
          auth.append(consume coded)
          req("Authorization") = consume auth
        end

        // Submit the request
        let sentreq = client(consume req, dumpMaker)
        // Could send body data via `sentreq`, if it was a POST
      else
        try env.out.print("Malformed URL: " + env.args(1)) end
      end
    else
      env.out.print("unable to use network")
    end

  be cancelled() =>
    """
    Process cancellation from the server end.
    """
    _env.out.print("-- response cancelled --")

  be have_response(response: Payload val) =>
    """
    Process return the the response message.
    """
    if response.status == 0 then
      _env.out.print("Failed")
      return
    end

    // Print the status and method
    _env.out.print(
      "Response " +
      response.status.string() + " " +
      response.method)

    // Print all the headers
    for (k, v) in response.headers().pairs() do
      _env.out.print(k + ": " + v)
    end

    _env.out.print("")

    // Print the body if there is any.  This will fail in Chunked or
    // Stream transfer modes.
    try
      let body = response.body()
      for piece in body.values() do
        _env.out.write(piece)
      end
    end

  be have_body(data: ByteSeq val)
    =>
    """
    Some additional response data.
    """
    _env.out.write(data)

  be finished() =>
    """
    End of the response data.
    """
    _env.out.print("-- end of body --")

class NotifyFactory is HandlerFactory
  """
  Create instances of our simple Receive Handler.
  """
  let _main: _GetWork

  new iso create(main': _GetWork) =>
    _main = main'

  fun apply(session: HTTPSession tag): HTTPHandler ref^ =>
    HttpNotify.create(_main, session)

class HttpNotify is HTTPHandler
  """
  Handle the arrival of responses from the HTTP server.  These methods are
  called within the context of the HTTPSession actor.
  """
  let _main: _GetWork
  let _session: HTTPSession tag

  new ref create(main': _GetWork, session: HTTPSession tag) =>
    _main = main'
    _session = session

  fun ref apply(response: Payload val) =>
    """
    Start receiving a response.  We get the status and headers.  Body data
    *might* be available.
    """
    _main.have_response(response)

  fun ref chunk(data: ByteSeq val) =>
    """
    Receive additional arbitary-length response body data.
    """
    _main.have_body(data)

  fun ref finished() =>
    """
    This marks the end of the received body data.
    """
    _main.finished()
    
  fun ref cancelled() =>
    _main.cancelled()
