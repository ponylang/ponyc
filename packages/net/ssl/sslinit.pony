use "path:/usr/local/opt/libressl/lib" if osx
use "lib:ssl"
use "lib:crypto"

primitive SSLInit
  new create() =>
    @SSL_load_error_strings[None]()
    @SSL_library_init[I32]()
