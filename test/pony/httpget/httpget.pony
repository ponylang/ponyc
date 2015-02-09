use "collections"
use "net/http"

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    // TODO: parse URLs from the command line
    try
      let url = URL(env.args(1))

      let client = Client(url.host, url.service)
      let req = Request("GET", url.path, recover this~apply() end)
      let req' = consume val req

      client(req')
    end

  be apply(request: Request, response: Response) =>
    _env.out.print("HTTP/1.1 " + response.status().string() + " " +
      response.status_text())

    try
      for (k, v) in response.headers().pairs() do
        _env.out.print(k + ": " + v)
      end

      _env.out.print("")
      _env.out.print(response.body())
    end
