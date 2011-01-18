#include <string.h>
#include <polarssl/ssl.h>
#include <polarssl/havege.h>

#include "tls.h"

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
tls_recv(int len, char *buf, int *remain, void *user)
{
  struct tls *tls = (struct tls *)user;
  int n;
  n = ssl_read(&tls->ssl, (unsigned char *)buf, len);
  *remain = ssl_get_bytes_avail(&tls->ssl);
  return n;
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
