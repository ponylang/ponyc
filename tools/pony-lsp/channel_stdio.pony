use "immutable-json"


class InputNotifier is InputNotify
  let handler: BaseProtocol

  new iso create(notifier: Notifier tag) =>
    handler = BaseProtocol(notifier)

  fun ref apply(data': Array[U8 val] iso): None val =>
    handler(consume data')

  fun ref dispose(): None val =>
    None

trait tag Channel
  be send(msg: Message val)
  be log(data: String val, message_type: MessageType = Debug)
  be set_notifier(notifier: Notifier tag)
  be dispose()

actor Stdio is Channel
  let out: OutStream
  let stdin: InputStream

  new create(
    out': OutStream tag,
    stdin': InputStream tag) =>
    out = out'
    stdin = stdin'

  be set_notifier(notifier: Notifier tag) =>
    // clear out any old handler
    this.stdin.dispose()

    // start receiving input
    // we do expect some bigger json messages, so allocate 256 bytes by default
    this.stdin.apply(InputNotifier(notifier) where chunk_size = 256)

  be send(msg: Message val) =>
    let output: String val = msg.string()
    out.write(output)
    out.flush()

  be log(data: String val, message_type: MessageType = Debug) =>
    """
    Log data to the editor via window/logMessage
    """
    send(
      Notification(
        "window/logMessage",
        Obj("type", message_type.apply())("message", data).build()
      )
    )
  be dispose() =>
    this.stdin.dispose()
