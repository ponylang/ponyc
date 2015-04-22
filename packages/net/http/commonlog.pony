use "time"

class CommonLog
  let _out: Stream

  new val create(out: Stream) =>
    _out = out

  fun val apply(ip: String, request: Payload, response: Payload) =>
    let list = recover Array[String](24) end

    list.push(ip)
    list.push(" - ")
    list.push(_entry(request.url.user))

    let time = Date(Time.seconds()).format("%d/%b/%Y:%H:%M:%S +0000")
    list.push(" [")
    list.push(time)
    list.push("] \"")

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
    list.push("\" ")
    list.push(response.status.string())
    list.push(" ")
    list.push(response.body_size().string())

    list.push(" \"")
    try list.push(request("Referrer")) end
    list.push("\" \"")
    try list.push(request("User-Agent")) end
    list.push("\"\n")

    _out.writev(consume list)

  fun _entry(s: String): String =>
    if s.size() > 0 then s else "-" end
