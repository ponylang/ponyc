class ContentsLog
  """
  Logs the contents of HTTP requests and responses.
  """
  let _out: Stream

  new val create(out: Stream) =>
    _out = out

  fun val apply(ip: String, request: Payload val, response: Payload val) =>
    let list = recover Array[Bytes] end

    list.push("REQUEST\n")
    list.push(request.method)
    list.push(" ")
    list.push(request.url.path)

    if request.url.query.size() > 0 then
      list.push("?")
      list.push(request.url.query)
    end

    if request.url.fragment.size() > 0 then
      list.push("#")
      list.push(request.url.fragment)
    end

    list.push(" ")
    list.push(request.proto)
    list.push("\n")

    for (k, v) in request.headers().pairs() do
      list.push(k)
      list.push(": ")
      list.push(v)
      list.push("\n")
    end

    for data in request.body().values() do
      list.push(data)
    end

    list.push("\n")

    list.push("RESPONSE\n")
    list.push(response.proto)
    list.push(" ")
    list.push(response.status.string())
    list.push(" ")
    list.push(response.method)
    list.push("\n")

    for (k, v) in response.headers().pairs() do
      list.push(k)
      list.push(": ")
      list.push(v)
      list.push("\n")
    end

    for data in response.body().values() do
      list.push(data)
    end

    list.push("\n\n")
    _out.writev(consume list)
