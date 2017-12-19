use "../../src/DBConnector"

class Foobar is DBNotify
  var _env: Env
  var _interval: F32 = 2
  new iso create(env': Env) =>
    _env = env'
    
  fun ref notification_received(n: String, m: String) =>
    _env.out.print("notification recieved: \"" + m + "\" from " + n)
  fun ref connection_established(s: String): F32=>
    _env.out.print("connection established: " + s)
    _interval = 0
    0
  fun ref connection_lost(s: String)=>
    _env.out.print("connection lost: " + s)

actor Main
  var _env: Env
  new create(env': Env) =>
    _env = env'
    let array = recover val [F32(2); F32(2); F32(2); F32(2); F32(2); F32(2); F32(2); F32(2); F32(2); F32(5); F32(10)] end
    let reconnectIntervalFn = { (x: F32): F32 => 
      var y = F32(0) 
      if x > 10 then 
        y = 15 
      else 
        y = try array(x.usize())? else 10 end
      end 
      _env.out.print(x.string() + "  -  " + y.string()) 
      y
    } val

    let dbConnect = DBConnector("dbname=nyxia connect_timeout=10", Foobar(env'))
    dbConnect.reconnect_interval(reconnectIntervalFn)
    //dbConnect.reconnect_interval_simple(F32(5))
    dbConnect.add_listen("tbl1")
    dbConnect.add_listen("testing")
    dbConnect.execute("notify tbl1, 'test'")
    dbConnect.execute("notify tbl1, ' 222'")