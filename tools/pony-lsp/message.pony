use "json"

trait Message is Stringable
  fun json(): JsonObject

  fun string(): String iso^ =>
    let content: String val = this.json().string()
    let size_str: String val = content.size().string()
      recover iso
        String.create(content.size() + size_str.size() + 20)
          .>append("Content-Length: ")
          .>append(size_str)
          .>append("\r\n\r\n")
          .>append(content)
      end

  fun into_bytes(): Array[U8] iso^ =>
    string().iso_array()


primitive Err
  fun tag apply(): I64 => 1
  fun string(): String => "ERROR"
primitive Warning
  fun tag apply(): I64 => 2
  fun string(): String => "WARNING"
primitive Info
  fun tag apply(): I64 => 3
  fun string(): String => "INFO"
primitive Log
  fun tag apply(): I64 => 4
  fun string(): String => "LOG"
primitive Debug
  fun tag apply(): I64 => 5
  fun string(): String => "DEBUG"

type MessageType is (Err | Warning | Info | Log | Debug)


type RequestId is (I64 | String)

primitive RequestIds
  fun eq(x: RequestId, y: RequestId): Bool =>
    match (x, y)
    | (let this_i: I64, let that_i: I64) => this_i == that_i
    | (let this_s: String, let that_s: String) => this_s == that_s
    else
      false
    end

class val RequestMessage is Message

  let id: RequestId
  let method: String
  let params: (JsonObject | JsonArray | None)

  new val create(
    id': RequestId,
    method': String,
    params': (JsonObject | JsonArray | None) = None)
  =>
    id = id'
    method = method'
    params = params'


  fun json(): JsonObject =>
    let obj = JsonObject
      .update("jsonrpc", "2.0")
      .update("id", id)
      .update("method", method)
    match this.params
    | let p: (JsonObject | JsonArray) =>
      obj.update("params", p)
    else
      obj
    end

class val Notification is Message

  let method: String
  let params: (JsonObject | JsonArray | None)

  new val create(
    method': String,
    params': (JsonObject | JsonArray | None) = None)
  =>
    method = method'
    params = params'

  fun json(): JsonObject =>
    JsonObject
      .update("jsonrpc", "2.0")
      .update("method", method)
      .update("params", this.params)

class val ResponseMessage is Message
  let id: (RequestId | None)
  let result: JsonValue
  let err: (ResponseError val | None)

  new val create(
    id': (RequestId | None),
    result': JsonValue,
    error': (ResponseError val | None) = None)
  =>
    """
    Constructor for a Response to a Request bearing a request id
    """
    id = id'
    result = result'
    err = error'

  fun is_err(): Bool =>
    """
    Returns `true` if this response denotes an error.
    """
    this.err isnt None

  fun is_result(): Bool =>
    """
    Returns `true` if we have a result, that is: A response denoting success for
    the provided `id`.
    """
    this.err is None

  fun json(): JsonObject =>
    let obj = JsonObject
      .update("jsonrpc", "2.0")
      .update("id", id)
    match \exhaustive\ this.err
    | let r: ResponseError val =>
      obj.update("error", r.json())
    | None =>
      obj.update("result", result)
    end


class val ResponseError
  let code: I64
  let message: String
  let data: JsonValue

  new val create(
    code': I64,
    message': String,
    data': JsonValue = None)
  =>
    code = code'
    message = message'
    data = data'

  fun json(): JsonObject =>
    JsonObject
      .update("code", code)
      .update("message", message)
      .update("data", data)


primitive ErrorCodes
  fun parse_error(): I64 => -32700
	fun invalid_request(): I64 => -32600
	fun method_not_found(): I64 => -32601
	fun invalid_params(): I64 => -32602
	fun internal_error(): I64 => -32603
  fun server_not_initialized(): I64 =>
    """
    Error code indicating that a server received a notification or
	  request before the server has received the `initialize` request.
    """
    -32002
	fun unknown_error_code(): I64 =>
    -32001
	fun request_failed(): I64 =>
    """
    A request failed but it was syntactically correct, e.g the
	  method name was known and the parameters were valid. The error
	  message should contain human readable information about why
	  the request failed.
    """
    -32803

	fun server_cancelled(): I64 =>
    """
    The server cancelled the request. This error code should
	  only be used for requests that explicitly support being
	  server cancellable.
    """
    -32802

	fun content_modified(): I64 =>
    """
    The server detected that the content of a document got
	  modified outside normal conditions. A server should
	  NOT send this error code if it detects a content change
	  in it unprocessed messages. The result even computed
	  on an older state might still be useful for the client.
	 
	  If a client decides that a result is not of any use anymore
	  the client should cancel the request.
    """
    -32801

	fun request_cancelled(): I64 =>
    """
    The client has canceled a request and a server has detected
	  the cancel.
    """
    -32800


trait tag RequestSender
  """
  Describes some entity that supports sending LSP requests.

  It needs to keep track of outgoing request ids.
  """
  be send_request(method: String val, params: (JsonObject | JsonArray | None))
