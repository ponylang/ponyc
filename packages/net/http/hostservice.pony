use "collections"

class HostService val is Hashable, Comparable[HostService]
  let host: String
  let service: String

  new val create(host': String, service': String) =>
    host = host'
    service = service'

  fun hash(): U64 =>
    host.hash() xor service.hash()

  fun eq(that: HostService box): Bool =>
    (host == that.host) and (service == that.service)
