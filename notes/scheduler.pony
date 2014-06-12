class Scheduler
  var queue: List[Actor]
  var waiting: (Scheduler|None)
  var ack: Bool

  fun ref run() =>
    while True do
      var act = getnext()
      if act.run() then queue.push_back(act) end
    end

  fun ref getnext(): Actor =>
    try
      var next = queue.pop_front()
      respond()
      next
    else
      request()
    end

  fun ref request(): Actor =>
    while True do
      if CAS(choose_victim().waiting, None, this) then
        while not ack do sleep() end
        try return queue.pop_front() end
      end
    end

  fun ref respond() =>
    match waiting
    | as thief: Actor =>
      try thief.queue.push_back(queue.pop_front()) end
      thief.ack = True
    | None => None
    end

class Actor
  var queue: MPSCQ[Msg]

  fun ref run(): Bool =>
    try
      var msg = queue.pop_front()
      // dispatch msg
      True
    else
      False
    end
