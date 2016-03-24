primitive NetAuth
  new create(from: AmbientAuth) =>
    None

primitive DNSAuth
  new create(from: (AmbientAuth | NetAuth)) =>
    None

primitive UDPAuth
  new create(from: (AmbientAuth | NetAuth)) =>
    None

primitive TCPAuth
  new create(from: (AmbientAuth | NetAuth)) =>
    None

primitive TCPListenAuth
  new create(from: (AmbientAuth | NetAuth | TCPAuth)) =>
    None

primitive TCPConnectAuth
  new create(from: (AmbientAuth | NetAuth | TCPAuth)) =>
    None
