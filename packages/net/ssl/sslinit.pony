use "path:/usr/local/opt/libressl/lib" if osx
use "lib:ssl" if not windows
use "lib:crypto" if not windows
use "lib:libssl-32" if windows
use "lib:libcrypto-32" if windows

primitive SSLInit
  new create() =>
    @SSL_load_error_strings[None]()
    @SSL_library_init[I32]()
