#include <string.h>
#include <polarssl/ssl.h>
#include <polarssl/havege.h>

#include "tls.h"

static void
my_debug(void *ctx, int level, const char *str)
{
  if (level < 4)
    fprintf(stderr, "tls dbg[%d]: %s\n", level, str);
}

static int
io_send(void *user, unsigned char *buf, int len)
{
  struct tls *tls = (struct tls *)user;
  return tls->send(len, (char *)buf, tls->user);
}

static int
io_recv(void *user, unsigned char *buf, int len)
{
  struct tls *tls = (struct tls *)user;
  return tls->recv(len, (char *)buf, tls->user);
}

int
tls_send(int len, const char *buf, void *user)
{
  struct tls *tls = (struct tls *)user;
  return ssl_write(&tls->ssl, (unsigned char *)buf, len);
}

int
tls_recv(int len, char *buf, void *user)
{
  struct tls *tls = (struct tls *)user;
  return ssl_read(&tls->ssl, (unsigned char *)buf, len);
}

int
tls_start(struct tls *tls)
{
  if (ssl_init(&tls->ssl))
    return -1;
  havege_init(&tls->hs);
  memset(&tls->ssn, 0, sizeof(tls->ssn));

  ssl_set_endpoint(&tls->ssl, SSL_IS_CLIENT);
  ssl_set_authmode(&tls->ssl, SSL_VERIFY_NONE);

  ssl_set_rng(&tls->ssl, havege_rand, &tls->hs);
#if 0
  ssl_set_dbg(&tls->ssl, my_debug, stderr);
#endif
  ssl_set_bio(&tls->ssl, io_recv, tls, io_send, tls);

  ssl_set_ciphers(&tls->ssl, ssl_default_ciphers);
  ssl_set_session(&tls->ssl, 0, 600, &tls->ssn);

  if (ssl_handshake(&tls->ssl)) {
    tls_cleanup(tls);
    return -1;
  }
  return 0;
}

void
tls_cleanup(struct tls *tls)
{
  ssl_close_notify(&tls->ssl);
  ssl_free(&tls->ssl);
}