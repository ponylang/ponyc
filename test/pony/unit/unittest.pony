use ponytest = "ponytest"
use collections = "collections"
use base64 = "encode/base64"
use net = "net"
use http = "net/http"
use options = "options"

actor Main
  new create(env: Env) =>
    ponytest.Main(env)
    collections.Main(env)
    base64.Main(env)
    net.Main(env)
    http.Main(env)
    options.Main(env)