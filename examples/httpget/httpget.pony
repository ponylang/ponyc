use "assert"
use "collections"
use "net/http"
use "net/ssl"
use "files"

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    let sslctx = try
      recover
        SSLContext
          .>set_client_verify(true)
          .>set_authority(FilePath(env.root as AmbientAuth, "cacert.pem"))
      end
    end

    try
      let client = Client(env.root as AmbientAuth, consume sslctx)

      for i in Range(1, env.args.size()) do
        try
          let url = URL.build(env.args(i))
          Fact(url.host.size() > 0)

          let req = Payload.request("GET", url, recover this~apply() end)
          client(consume req)
        else
          try env.out.print("Malformed URL: " + env.args(i)) end
        end
      end
    else
      env.out.print("unable to use network")
    end

  be apply(request: Payload val, response: Payload val) =>
    if response.status != 0 then
      // TODO: aggregate as a single print
      _env.out.print(
        response.proto + " " +
        response.status.string() + " " +
        response.method)

      for (k, v) in response.headers().pairs() do
        _env.out.print(k + ": " + v)
      end

      _env.out.print("")

      for chunk in response.body().values() do
        _env.out.write(chunk)
      end

      _env.out.print("")
    else
      _env.out.print("Failed: " + request.method + " " + request.url.string())
    end
