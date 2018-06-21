use "path:/usr/local/opt/libressl/lib" if osx
use "lib:ssl"
use "lib:crypto"

use @OPENSSL_init_ssl[I32](opts: U64, settings: Pointer[_OpenSslInitSettings])
use @OPENSSL_INIT_new[Pointer[_OpenSslInitSettings]]()

primitive _OpenSslInitSettings

// From https://github.com/ponylang/ponyc/issues/330
primitive _OpenSslInitNoLoadSslStrings    fun val apply(): U64 => 0x00100000
primitive _OpenSslInitLoadSslStrings      fun val apply(): U64 => 0x00200000
primitive _OpenSslInitNoLoadCryptoStrings fun val apply(): U64 => 0x00000001
primitive _OpenSslInitLoadCryptoStrings   fun val apply(): U64 => 0x00000002

primitive _SSLInit
  """
  This initialises SSL when the program begins.
  """
  fun _init() =>
    ifdef "openssl_1.1.0" then
      let settings = @OPENSSL_INIT_new()
      @OPENSSL_init_ssl(_OpenSslInitLoadSslStrings.apply()
          + _OpenSslInitLoadCryptoStrings.apply(), settings)
    else
      @SSL_load_error_strings[None]()
      @SSL_library_init[I32]()
      let cb =
        @ponyint_ssl_multithreading[Pointer[U8]](@CRYPTO_num_locks[I32]())
      @CRYPTO_set_locking_callback[None](cb)
    end
