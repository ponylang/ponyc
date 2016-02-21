use "net"
use "net/ssl"

actor Main
  new create(env: Env) =>
    var i = USize(1)

    let ssl = try
      if env.args(i) == "ssl" then
        i = i + 1
        true
      else
        false
      end
    else
      false
    end

    let limit = try
      env.args(i).usize()
    else
      1
    end

    match env.root
    | let r: AmbientAuth =>
      let network = NetworkInterface(r)
      try
        network.listen(recover Listener(env, ssl, limit) end)
        network.udp_socket(recover Pong(env) end)
      end
    else
      env.out.print("no root in Env!")
    end
