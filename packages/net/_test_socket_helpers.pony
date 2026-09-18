use @socket[I32](domain: I32, sock_type: I32, protocol: I32)

primitive \nodoc\ _AF
  fun inet(): I32 =>
    ifdef haiku then 1
    else 2
    end

primitive \nodoc\ _SOCK
  fun stream(): I32 => 1
  fun dgram(): I32 => 2

primitive \nodoc\ _SO
  fun none(): I32 => 0
