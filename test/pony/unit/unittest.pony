use ponytest = "ponytest"
use base64 = "encode/base64"
use net = "net"
use http = "net/http"
use collections = "collections"

actor Main
  new create(env: Env) =>
    ponytest.Main(env)
    base64.Main(env)
    net.Main(env)
    http.Main(env)
    collections.Main(env)
