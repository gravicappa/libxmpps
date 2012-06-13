/* Copyright 2010-2011 Ramil Farkhshatov

This file is part of libxmpps.

libxmpps is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

libxmpps is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libxmpps.  If not, see <http://www.gnu.org/licenses/>. */

#include <string.h>
#include <polarssl/ssl.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/entropy.h>

#include "tls.h"

const char *pers = "libxmpps";

static int
io_send(void *user, const unsigned char *buf, size_t len)
{
  struct tls *tls = (struct tls *)user;
  return tls->send(len, (char *)buf, tls->user);
}

static int
io_recv(void *user, unsigned char *buf, size_t len)
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
  if (n < 0)
    fprintf(stderr, "err: ssl_read => -0x%x\n", -n);
  *remain = ssl_get_bytes_avail(&tls->ssl);
  return n;
}

int
tls_start(struct tls *tls)
{
  memset(&tls->ssn, 0, sizeof(ssl_session));
  memset(&tls->ssl, 0, sizeof(ssl_context));
  entropy_init(&tls->entropy);
  if (ctr_drbg_init(&tls->ctr_drbg, entropy_func, &tls->entropy, pers,
                    strlen(pers)))
    return -1;

  if (ssl_init(&tls->ssl))
    return -1;

  ssl_set_endpoint(&tls->ssl, SSL_IS_CLIENT);
  ssl_set_authmode(&tls->ssl, SSL_VERIFY_NONE);

  ssl_set_rng(&tls->ssl, ctr_drbg_random, &tls->ctr_drbg);
  ssl_set_bio(&tls->ssl, io_recv, tls, io_send, tls);

  ssl_set_ciphersuites(&tls->ssl, ssl_default_ciphersuites);
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
  memset(&tls->ssl, 0, sizeof(tls->ssl));
}
