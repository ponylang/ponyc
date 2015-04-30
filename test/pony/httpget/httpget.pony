use "collections"
use "net/http"
use "net/ssl"

actor Main
  let _env: Env
  let _client: Client

  new create(env: Env) =>
    SSLInit
    _env = env

    let sslctx = try
      recover
        SSLContext
          .set_client_verify(true)
          .set_authority("./test/pony/httpget/cacert.pem")
      end
    end

    _client = Client(consume sslctx)

    for i in Range(1, env.args.size()) do
      try
        let url = URL.build(env.args(i))
        Fact(url.host.size() > 0)

        let req = Payload.request("GET", url, recover this~apply() end)
        _client(consume req)
      else
        try env.out.print("Malformed URL: " + env.args(i)) end
      end
    end

  be apply(request: Payload val, response: Payload val) =>
    if response.status != 0 then
      // TODO: aggregate as a single print
      _env.out.print(
        response.proto + " " +
        response.status.string() + " " +
        response.method)

      try
        for (k, v) in response.headers().pairs() do
          _env.out.print(k + ": " + v)
        end

        _env.out.print("")

        for chunk in response.body().values() do
          _env.out.write(chunk)
        end

        _env.out.print("")
      end
    else
      _env.out.print("Failed: " + request.method + " " + request.url.string())
    end
