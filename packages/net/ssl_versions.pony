primitive SSLAutoVersion
  """
  Let the SSL library choose the version. As a minimum it is the lowest version
  the library will negotiate, and as a maximum, the highest.
  """
  fun val apply(): ULong => 0x0

primitive SSL3Version
  """
  SSL 3.0.
  """
  fun val apply(): ULong => 0x300

primitive TLS1Version
  """
  TLS 1.0.
  """
  fun val apply(): ULong => 0x301

primitive TLS1u1Version
  """
  TLS 1.1.
  """
  fun val apply(): ULong => 0x302

primitive TLS1u2Version
  """
  TLS 1.2.
  """
  fun val apply(): ULong => 0x303

primitive TLS1u3Version
  """
  TLS 1.3.
  """
  fun val apply(): ULong => 0x304

primitive DTLS1Version
  """
  DTLS 1.0.
  """
  fun val apply(): ULong => 0xFEFF

primitive DTLS1u2Version
  """
  DTLS 1.2.
  """
  fun val apply(): ULong => 0xFEFD

primitive TLSMinVersion
  """
  The lowest TLS version this package names.
  """
  fun val apply(): ULong => TLS1Version()

primitive TLSMaxVersion
  """
  The highest TLS version this package names.
  """
  fun val apply(): ULong => TLS1u3Version()

primitive DTLSMinVersion
  """
  The lowest DTLS version this package names.
  """
  fun val apply(): ULong => DTLS1Version()

primitive DTLSMaxVersion
  """
  The highest DTLS version this package names.
  """
  fun val apply(): ULong => DTLS1u2Version()
