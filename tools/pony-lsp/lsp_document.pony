/*
use "json"
use "collections"
use "files"
use "backpressure"
use "process"
use "random"
use "debug"
use "pony_compiler"


actor DocumentProtocol
  var initialized: Bool = false
  let channel: Stdio
  let compiler: PonyCompiler


  new create(compiler': PonyCompiler, channel': Stdio) =>
    channel = channel'
    compiler = compiler'


  be handle_did_open(msg: RequestMessage val) =>
    let errors_notifier = ErrorsNotifier(channel)
    match msg.params
    | let p: JsonObject =>
      try
        let text_document = p("textDocument")? as JsonObject
        let uri = text_document("uri")? as String val
        let text = text_document("text")? as String val
        let filepath = uri.clone()
        filepath.replace("file://", "")
        Debug.err("DocumentProtocol calling compiler to check " + filepath.clone())
        errors_notifier.track_file(filepath.clone())
        compiler(consume filepath, errors_notifier)
      else
        Debug.err("ERROR retrieving textDocument uri: " + msg.json().string())
      end
    end

  be handle_did_save(msg: RequestMessage val) =>
    let errors_notifier = ErrorsNotifier(channel)
    match msg.params
    | let p: JsonObject =>
      try
        let text_document = p("textDocument")? as JsonObject
        let uri = text_document("uri")? as String val
        let filepath = uri.clone()
        filepath.replace("file://", "")
        Debug.err("DocumentProtocol calling compiler to check " + filepath.clone())
        errors_notifier.track_file(filepath.clone())
        compiler(consume filepath, errors_notifier)
      else
        Debug.err("ERROR retrieving textDocument uri: " + msg.json().string())
      end
    end


actor ErrorsNotifier
  let channel: Stdio
  let errors: Map[String, Array[JsonObject val]] = Map[String, Array[JsonObject val]]

  new create(channel': Stdio) =>
    channel = channel'

  be track_file(filepath: String) =>
    let errorlist = try errors(filepath)? else Array[JsonObject val] end
    errors(filepath) = errorlist

  be on_error(filepath: (String | None), line: USize, pos: USize, msg: String) =>
    var uri: String = "no_file"
    match filepath
    | let f: String => uri = "file://" + f
    end
    var errorlist = try errors(uri)? else Array[JsonObject val] end
    errorlist.push(
      JsonObject
        .update("message", msg)
        .update("range", JsonObject
          .update("start", JsonObject
            .update("line", line.i64()-1)
            .update("character", pos.i64()))
          .update("end", JsonObject
            .update("line", line.i64()-1)
            .update("character", pos.i64())))
    )
    errors(uri) = errorlist

  be done() =>
    for i in errors.keys() do
      let errorlist: Array[JsonValue] iso = []
      try
        for e in errors(i)?.values() do
          errorlist.push(e)
        end
      end
      var diagnostics_arr = JsonArray
      for e in (consume val errorlist).values() do
        diagnostics_arr = diagnostics_arr.push(e)
      end
      channel.send_message(RequestMessage(None, "textDocument/publishDiagnostics",
        JsonObject
          .update("uri", i)
          .update("diagnostics", diagnostics_arr)
      ))
    end
*/
