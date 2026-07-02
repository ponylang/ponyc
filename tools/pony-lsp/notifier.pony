// TODO: self shutdown when editor crashes
// To support the case that the editor starting a
// server crashes an editor should also pass its
// process id to the server. This allows the server to
// monitor the editor process and to shutdown itself if
// the editor process dies. The process id pass on the
// command line should be the same as the one passed in
// the initialize parameters. The command line argument
// to use is --clientProcessId.

use "json"
use "collections"
use "buffered"

type ReceivingMode is (ReceivingModeHeader | ReceivingModeContent)

primitive ReceivingModeHeader
  """
  Protocol is receiving header lines.
  """

primitive ReceivingModeContent
  """
  Protocol is receiving message content.
  """

primitive InvalidJSON
  """
  Parse error for invalid JSON content.
  """
  fun string(): String val => "Invalid JSON"

primitive NoContentLength
  """
  Parse error for missing Content-Length header.
  """
  fun string(): String val =>
    "No Content-Length header"

primitive InvalidContentLength
  """
  Parse error for invalid Content-Length header.
  """
  fun string(): String val =>
    "Invalid Content-Length header"

class val InvalidMessage
  """
  Parse error for an invalid LSP message.
  """
  let msg: String

  new val create(msg': String) =>
    msg = msg'

  fun string(): String val =>
    "Invalid Message: " + msg

type ParseError is
  (InvalidJSON | NoContentLength | InvalidContentLength | InvalidMessage)

primitive NeedMore
  """
  Indicates more data is needed to parse.
  """

interface Notifier
  """
  Callback interface for parsed LSP messages.
  """
  be handle_message(msg: Message val)
    """
    Handle a parsed LSP message.
    """

  be handle_parse_error(parse_error: ParseError)
    """
    Handle a parse error.
    """

class ref BaseProtocol
  """
  Implements the LSP base protocol framing over a byte
  stream, parsing headers and JSON content.
  """
  let buffer: Reader = Reader
  let notifier: Notifier tag
  var headers: Map[String, String] = Map[String, String]
  var receiving_mode: ReceivingMode = ReceivingModeHeader

  new ref create(notifier': Notifier tag) =>
    notifier = notifier'

  fun ref apply(data: Array[U8] iso) =>
    buffer.append(consume data)
    var res = parse()
    while res isnt NeedMore do
      match res
      | let m: Message val => notifier.handle_message(m)
      | let pe: ParseError => notifier.handle_parse_error(pe)
      end
      // cleanup between messages/errors
      this.headers.clear()
      this.receiving_mode = ReceivingModeHeader
      res = parse()
    end

  fun ref parse(): (Message val | ParseError | NeedMore) =>
    match \exhaustive\ receiving_mode
    | ReceivingModeHeader => receive_headers()
    | ReceivingModeContent => receive_content()
    end

  fun ref receive_headers(): (Message val | ParseError | NeedMore) =>
    try
      // read as many headers as we can
      var msg = this.buffer.line()?
      while true do
        // End Of Headers
        if msg.size() == 0 then
          receiving_mode = ReceivingModeContent
          return receive_content()
        else
          let hdata = msg.split_by(": ", 2)
          this.headers.insert(hdata(0)?, hdata(1)?)
        end
        msg = this.buffer.line()?
      end
      NeedMore // massaging the return type
    else
      // need more data
      NeedMore
    end

  fun ref receive_content(): (Message val | ParseError | NeedMore) =>
    """
    Parse the message content from the buffer.
    """
    let cl_str =
      try
        headers("Content-Length")?
      else
        return NoContentLength
      end
    let content_length =
      try
        cl_str.usize()?
      else
        return InvalidContentLength
      end
    let content: Array[U8] val =
      try
        this.buffer.block(content_length)?
      else
        return NeedMore
      end
    let message_json: JsonObject =
      match JsonParser.parse(
        String.from_array(content))
      | let obj: JsonObject => obj
      else
        // syntactically invalid json or not an object
        return InvalidJSON
      end
    parse_message(message_json)

  fun ref parse_message(json: JsonObject): (Message val | ParseError) =>
    if json.contains("method") then
      let method: String =
        try
          match json("method")?
          | let s: String => s
          else
            return InvalidMessage("Invalid method. Not a string.")
          end
        else
          return InvalidMessage("Missing method")
        end
      // params are optional, but if present must be
      // object or array
      let params: (JsonObject | JsonArray | None) =
        try
          match json("params")?
          | let obj: JsonObject => obj
          | let arr: JsonArray => arr
          else
            return InvalidMessage("Invalid request params")
          end
        else
          None
        end
      if json.contains("id") then
        // request
        let id: RequestId =
          try
            match json("id")?
            | let id: String => id
            | let id: I64 => id
            else
              return InvalidMessage("Invalid id. Expected string or integer")
            end
          else
            return InvalidMessage("Missing request id")
          end
        RequestMessage(id, method, params)
      else
        // notification
        Notification(method, params)
      end
    else
      // response
      let result: JsonValue =
        try
          json("result")?
        end
      let id: (RequestId | None) =
        try
          match json("id")?
          | let id: String => id
          | let id: I64 => id
          else
            return InvalidMessage("Invalid id. Expected string or integer")
          end
        else
          None
        end
      let response_error:
        (ResponseError val | None) =
          try
            match json("error")?
            | let err: JsonObject =>
              // parse ResponseError object
              match \exhaustive\ parse_response_error(err)
              | let pe: ParseError => return pe
              | let re: ResponseError => re
              end
            else
              return InvalidMessage("Invalid response error")
            end
          else
            None
          end
      ResponseMessage.create(id, result, response_error)
    end

  fun parse_response_error(json: JsonObject):
    (ResponseError val | ParseError)
  =>
    """
    Parse a ResponseError from a JSON object.
    """
    let code: I64 =
      try
        match json("code")?
        | let c: I64 => c
        else
          return InvalidMessage("Invalid ResponseError code. Not an integer.")
        end
      else
        return InvalidMessage("Missing ResponseError code")
      end
    let message: String =
      try
        match json("message")?
        | let m: String => m
        else
          return InvalidMessage("Invalid ResponseError message. Not a string.")
        end
      else
        return InvalidMessage("Missing ResponseError message")
      end
    let data: JsonValue =
      try
        json("data")?
      end
    ResponseError(code, message, data)
