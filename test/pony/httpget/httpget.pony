use "collections"
use "net/http"

actor Main
  let _clients: Map[HostService, Client] = _clients.create()
  let _env: Env

  new create(env: Env) =>
    _env = env

    for i in Range(1, env.args.size()) do
      try
        let url = URL(env.args(i))
        Fact(url.scheme == "http")
        Fact(url.host != "")

        let client = _get_client(url.host, url.service)
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
        _env.out.print(response.body())
      end
    else
      _env.out.print("Failed: " + request.method() + " " + request.resource())
    end

  fun ref _get_client(host: String, service: String = "80"): Client =>
    let hs = if service.size() == 0 then
      HostService(host, "80")
    else
      HostService(host, service)
    end

    try
      _clients(hs)
    else
      let client = Client(hs.host, hs.service)
      _clients(hs) = client
      client
    end
