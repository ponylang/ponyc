use "lib:pq"
use "time"
use "signals"
use "collections"
use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32, flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_destroy[None](event: AsioEventID)
use @PQsocket[U32](conn: PGconn)
use @PQexec[PGresult](conn: PGconn, query: Pointer[U8] tag)
use @PQconnectdb[PGconn](conn_str:Pointer[U8] tag)
use @PQnotifies[MaybePointer[PGnotify]](conn: PGconn)
use @PQconsumeInput[I32](conn: PGconn)

primitive PGresult
primitive PGconn

struct PGnotify
  var relname: Pointer[U8] = Pointer[U8]
  var be_pid: I32 = -1
  var extra: Pointer[U8] = Pointer[U8]
  new create() => None

interface DBNotify
  fun ref notification_received(notify: String, message:String)
  fun ref connection_lost(s: String)
  fun ref connection_established(s: String): F32

actor DBConnector
  var _conn: PGconn
  var _fd: U32
  let _connInfo: String
  var _event: AsioEventID = AsioEventID
  var _count: F32 = 0
  var _ready2send: Bool = false
  var _reconnectIntervals: F32 = 0
  var _subscribedChannels: Set[String] = Set[String]
  var _notifier: DBNotify iso
  var _reconnectIntervalFn: {(F32): F32} val = { (x: F32): F32 => x }

    new create(connInfo: String, notifier': DBNotify iso) =>
    _notifier = consume notifier'
    _connInfo = connInfo
    _conn = @PQconnectdb((_connInfo).cstring())
    _fd = @PQsocket(_conn)
    this.connect(true)
  
  fun _listen(channel: String):PGresult =>
    @PQexec(_conn, ("listen " + channel).cstring())

  fun _unlisten(channel: String):PGresult =>
    @PQexec(_conn, ("unlisten " + channel).cstring())

  be execute(s: String) =>
    if _ready2send then
    @PQexec(_conn, (s).cstring())
    else
      let timers = Timers
      let timer = Timer(ExecuteNotReady(this, s), 1_000_000)
      timers(consume timer)
    end

  be reconnect_interval(f: {(F32): F32} val) =>
    _reconnectIntervalFn = f

  be reconnect_interval_simple(f: F32) =>
    _reconnectIntervalFn = { (x: F32): F32 => f }

  be add_listen(channel: String) =>
    _subscribedChannels = _subscribedChannels.add(channel)
    _listen(channel)
  
  be removeListen(channel: String) =>
    _unlisten(channel)
    _subscribedChannels = _subscribedChannels.sub(channel)

  be connect(initial: Bool = false) =>
    if not initial then
      _conn = @PQconnectdb((_connInfo).cstring())
      _fd = @PQsocket(_conn)
    end
    let status = @PQstatus[I32](_conn)
    if status != 0 then
      _reconnectIntervals = _reconnectIntervalFn(_count)
      _count = _count + 1
      this.tryConnect(_reconnectIntervals)
    else
      _reconnectIntervals = _notifier.connection_established(_connInfo)
      _ready2send = true
      for channel in _subscribedChannels.values() do
        _listen(channel)
      end
      @pony_asio_event_create(this, _fd, AsioEvent.read(), 0, true)
    end

  be tryConnect(length: F32 = 1) =>
    @PQconsumeInput(_conn)
    let notify = @PQnotifies(_conn)
    if notify.is_none() then
      let timers = Timers
      let timer = Timer(Notify(this), (length * 1_000_000_000).u64())
      timers(consume timer)
    end

  be dispose() =>
    @PQfinish[None](_conn)
    @pony_asio_event_destroy(_event)

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    if AsioEvent.disposable(flags) then
      @pony_asio_event_destroy(event)
      return
    end
    var status = @PQconsumeInput(_conn)
    if status != 1 then
      _ready2send = false
      _count = 0
      this.tryConnect(_reconnectIntervalFn(_count))
      _notifier.connection_lost(_connInfo)
    end
    try
      let notify = @PQnotifies(_conn)
      _notifier.notification_received(String.from_cstring(notify()?.relname).string(), String.from_cstring(notify()?.extra).string())
      @PQfreemem[None](notify)
    end

class Notify is TimerNotify
  let _acting: DBConnector

  new iso create(acting: DBConnector) =>
    _acting = acting

  fun ref apply(timer: Timer, count: U64): Bool =>
    _acting.connect()
    false

class ExecuteNotReady is TimerNotify
  let _acting: DBConnector
  let _s: String

  new iso create(acting: DBConnector, s: String) =>
    _acting = acting
    _s = s

  fun ref apply(timer: Timer, count: U64): Bool =>
    _acting.execute(_s)
    false
