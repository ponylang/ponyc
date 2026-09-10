primitive TLSAuthFailed
  """
  The TLS handshake failed due to an authentication error (certificate
  validation failure, untrusted CA, hostname mismatch, etc.).
  """

primitive TLSGeneralError
  """
  The TLS handshake failed due to a protocol error other than authentication
  (unexpected message, unsupported version, internal SSL error, etc.).
  """

type TLSFailureReason is (TLSAuthFailed | TLSGeneralError)
