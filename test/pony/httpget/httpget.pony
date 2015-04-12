use "collections"
use "net/http"
use "net/ssl"

actor Main
  let _clients: Map[HostService, Client] = _clients.create()
  let _env: Env

  new create(env: Env) =>
    SSLInit
    _env = env

    for i in Range(1, env.args.size()) do
      try
        let url = URL(env.args(i))
        Fact(url.host != "")

        let client = _get_client(url.scheme, url.host, url.service)
        let req = Request("GET", url.path, recover this~apply() end)
        client(consume req)
      else
        try env.out.print("Malformed URL: " + env.args(i)) end
      end
    end

  be apply(request: Request, response: Response) =>
    if response.status() != 0 then
      _env.out.print(
        response.proto() + " " +
        response.status().string() + " " +
        response.status_text())

      try
        for (k, v) in response.headers().pairs() do
          _env.out.print(k + ": " + v)
        end

        _env.out.print("")

        for chunk in response.body().values() do
          _env.out.print(chunk)
        end
      end
    else
      _env.out.print("Failed: " + request.method() + " " + request.resource())
    end

  fun ref _get_client(scheme: String, host: String, service: String): Client ?
  =>
    let port = if service.size() == 0 then
      match scheme
      | "http" => "80"
      | "https" => "443"
      else
        error
      end
    else
      service
    end

    let hs = HostService(scheme, host, port)

    try
      _clients(hs)
    else
      let client = match scheme
      | "http" => Client(hs.host, hs.service)
      | "https" => Client(hs.host, hs.service, true)
      else
        error
      end

      _clients(hs) = client
      client
    end
