interface TCPConnectionNotify
  """
  Notifications for TCP connections.
  """
  fun ref accepted(conn: TCPConnection ref) =>
    None

  fun ref connecting(conn: TCPConnection ref) =>
    None

  fun ref connected(conn: TCPConnection ref) =>
    None

  fun ref connect_failed(conn: TCPConnection ref) =>
    None

  fun ref read(conn: TCPConnection ref, data: Array[U8] iso) =>
    None

  fun ref closed(conn: TCPConnection ref) =>
    None

interface TCPListenNotify
  """
  Notifications for TCP listeners.
  """
  fun ref listening(listen: TCPListener ref) =>
    None

  fun ref not_listening(listen: TCPListener ref) =>
    None

  fun ref connection(listen: TCPListener ref): TCPConnectionNotify iso^
