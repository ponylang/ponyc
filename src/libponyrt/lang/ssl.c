#include <platform.h>
#include <openssl/ssl.h>

#if defined(PLATFORM_IS_WINDOWS)
static HANDLE* locks;
#else
static pthread_mutex_t* locks;
#endif


PONY_EXTERN_C_BEGIN

// Forward declaration to avoid C++ name mangling
void* ponyint_ssl_multithreading(uint32_t count);
void* ponyint_ssl_ctx_accessor(SSL_CTX *ctx, uint32_t element);
void ponyint_ssl_ctx_clear_extra_certs(SSL_CTX *ctx);

PONY_EXTERN_C_END


static void locking_callback(int mode, int type, const char* file, int line)
{
  (void)file;
  (void)line;

  if(mode & 1)
  {
#if defined(PLATFORM_IS_WINDOWS)
    WaitForSingleObject(locks[type], INFINITE);
#else
    pthread_mutex_lock(&locks[type]);
#endif
  } else {
#if defined(PLATFORM_IS_WINDOWS)
    ReleaseMutex(locks[type]);
#else
    pthread_mutex_unlock(&locks[type]);
#endif
  }
}

void* ponyint_ssl_multithreading(uint32_t count)
{
#if defined(PLATFORM_IS_WINDOWS)
  locks = (HANDLE*)malloc(count * sizeof(HANDLE*));

  for(uint32_t i = 0; i < count; i++)
    locks[i] = CreateMutex(NULL, FALSE, NULL);
#else
  locks = malloc(count * sizeof(pthread_mutex_t));

  for(uint32_t i = 0; i < count; i++)
    pthread_mutex_init(&locks[i], NULL);
#endif

  return locking_callback;
}

#define DEFAULT_PASSWD_CALLBACK 0
#define DEFAULT_PASSWD_CALLBACK_USERDATA 1

void* ponyint_ssl_ctx_accessor(SSL_CTX *ctx, uint32_t element)
{
  switch (element)
  {
  case DEFAULT_PASSWD_CALLBACK:
    return ctx->default_passwd_callback;
  case DEFAULT_PASSWD_CALLBACK_USERDATA:
    return ctx->default_passwd_callback_userdata;
  default:
    return 0;
  }
}

/*
 * OpenSSL later vesions (untested):
 * var ret = @SSL_CTX_clear_chain_certs[I32](_ctx)
 * if ret == 0 then
 *   _SSLDebug()
 *   error
 * end
*/
void ponyint_ssl_ctx_clear_extra_certs(SSL_CTX *ctx)
{
  if (ctx->extra_certs != NULL) {
    sk_X509_pop_free(ctx->extra_certs, X509_free);
    ctx->extra_certs = NULL;
  }
}
