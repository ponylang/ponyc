use "lib:ssl"
use "lib:crypto"

use @OPENSSL_init_ssl[I32](opts: U64, settings: Pointer[_OpenSSLInitSettings])
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
use @OPENSSL_INIT_new[Pointer[_OpenSSLInitSettings]]()
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x"
use @OPENSSL_INIT_free[None](settings: Pointer[_OpenSSLInitSettings])
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x"

primitive _OpenSSLInitSettings

// From https://github.com/ponylang/ponyc/issues/330
primitive _OpenSSLInitNoLoadSSLStrings    fun val apply(): U64 => 0x00100000
primitive _OpenSSLInitLoadSSLStrings      fun val apply(): U64 => 0x00200000
primitive _OpenSSLInitNoLoadCryptoStrings fun val apply(): U64 => 0x00000001
primitive _OpenSSLInitLoadCryptoStrings   fun val apply(): U64 => 0x00000002

primitive _SSLInit
  """
  This initialises SSL when the program begins.
  """
  fun _init() =>
    ifdef "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" then
      let settings = @OPENSSL_INIT_new()
      @OPENSSL_init_ssl(
        _OpenSSLInitLoadSSLStrings() + _OpenSSLInitLoadCryptoStrings(),
        settings)
      @OPENSSL_INIT_free(settings)
    elseif "libressl" then
      @OPENSSL_init_ssl(0, Pointer[_OpenSSLInitSettings])
    else
      compile_error "You must select an SSL version to use."
    end
