use @pony_exitcode[None](code: I32)
use @usleep[I32](micros: U32) if not windows
use @Sleep[None](millis: U32) if windows
use "actor_pinning"

actor Main
  let _env: Env
  let _auth: PinUnpinActorAuth

  new create(env: Env) =>
    _env = env
    _auth = PinUnpinActorAuth(env.root)
    ActorPinning.request_pin(_auth)
    check_pinned()

  be check_pinned() =>
    if ActorPinning.is_successfully_pinned(_auth) then
      do_stuff(100)
    else
      check_pinned()
    end

  be do_stuff(i: I32) =>
    // sleep for a while so that the quiescence CNF/ACK protocol can happen
    ifdef windows then
      @Sleep(10)
    else
      @usleep(10000)
    end
    if i < 0 then
      // set the exit code if this behavior has been run enough times
      // issue 4582 identified early quiescence/termination if only pinned
      // actors remained active
      @pony_exitcode(100)
    else
      do_stuff(i - 1)
    end

  be done() =>
    ActorPinning.request_unpin(_auth)