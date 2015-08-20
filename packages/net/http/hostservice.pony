use "collections"

class _HostService val is (Hashable & Comparable[_HostService])
  let scheme: String
  let host: String
  let service: String

  new val create(scheme': String, host': String, service': String) =>
    scheme = scheme'
    host = host'
    service = service'

  fun hash(): U64 =>
    scheme.hash() xor host.hash() xor service.hash()

  fun eq(that: _HostService box): Bool =>
    (scheme == that.scheme) and
      (host == that.host) and
      (service == that.service)
