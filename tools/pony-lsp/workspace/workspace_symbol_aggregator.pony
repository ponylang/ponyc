use "json"
use ".."

actor WorkspaceSymbolAggregator
  """
  Collects workspace/symbol results from multiple WorkspaceManager actors
  and sends a single aggregated response once all workspaces have replied.
  """
  let _channel: Channel
  let _request_id: RequestId
  var _remaining: USize
  var _results: JsonArray

  new create(channel: Channel, request_id: RequestId, workspace_count: USize) =>
    _channel = channel
    _request_id = request_id
    _remaining = workspace_count
    _results = JsonArray

  be add_results(symbols: JsonArray) =>
    for s in symbols.values() do
      _results = _results.push(s)
    end
    _remaining = _remaining - 1
    if _remaining == 0 then
      _channel.send(ResponseMessage(_request_id, _results))
    end
