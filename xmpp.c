#include <stdio.h>
#include "pool.h"
#include "node.h"
#include "xml.h"
#include "xml_states.h"
#include "xmpp.h"

#define SASL_CNONCE_LEN 4

statict const char xmpp_ns_sasl[] = "urn:ietf:params:xml:ns:xmpp-sasl";
static const char xmpp_head_fmt[] = 
    "<?xml version='1.0'?> <stream:stream "
    "xmlns:stream='http://etherx.jabber.org/streams' xmlns='jabber:client'"
    "to='%s' version='1.0'>";

int
xmpp_process_input(int bytes, const char *buf, struct xmpp *xmpp, void *user)
{
  int i;
  char *id;

  for (i = 0; i < bytes; i++) {
    if (xml_next_char(buf[i], &xmpp->xml))
      return -1;

    switch (xmpp->xmp.state) {
    case XML_STATE_NODE_HEAD:
      if (xmpp->xml.level != 1)
        break;

      id = pool_ptr(&xmpp->xml.mem, xmpp->xml.node_id);
      break;
    case XML_STATE_NODE_END:
      if (xmpp->xml.level != 1)
        break;

      if (xmpp->xml.last_node != POOL_NIL
          && xmpp->node_fn && xmpp->node_fn(xmpp->xml.last_node, user))
        return -1;
      break;
    }
  }
  return 0;
}

int
xmpp_send_node(int node, struct xmpp *xmpp)
{
  int mark, bytes, ret = -1;
  char *s;

  mark = pool_state(&xmpp->mem);
  s = str_from_xml_node(&xmpp->mem, node, &xmpp->mem);
  if (s) {
    bytes = pool_state(&xmpp->mem) - mark;
    ret = io->send(bytes, s, xmpp->io);
  }
  pool_restore(&xmpp->mem, mark);
  return ret;
}

int
xmpp_start_stream(const char *to, struct xmpp *xmpp)
{
  int mark, h, len, ret = -1;
  char *s;

  mark = pool_state(&xmpp->mem);
  h = xml_printf(&xmpp->mem, POOL_NIL, xmpp_head_fmt, to);
  s = pool_ptr(&xmpp->mem, h);
  if (s)
    ret = xmpp->io->send(strlen(s), s, xmpp->io);
  pool_restore(&xmpp->mem, mark);
  return ret;
}

int
xmpp_sasl_mechanisms(int x, struct pool *p)
{
  int ret = 0;
  struct xml_data *d;
  char *s;

  for (d = xml_node_data(x, p); d; d = xml_data_next(d, p)) {
    if (d->type != XML_NODE)
      continue;
    s = xml_node_name(d->value, p);
    if (!s || strcmp(s, "mechanism"))
      continue;
    s = xml_node_text(d->value, p);
    if (!s)
      continue;
    if (!strcmp(s, "DIGEST-MD5"))
      ret |= XMPP_SASL_MD5;
    else if (!strcmp(s, "PLAIN"))
      ret |= XMPP_SASL_PLAIN;
  }
  return ret;
}

int
xmpp_stream_features(int x, struct pool *p) 
{
  struct xml_data *d;
  int ret = 0;
  char *s;

  s = xml_node_name(x);
  if (strcmp(s, "stream:features"))
    return 0;
  for (d = xml_node_data(x, p); d; d = xml_data_next(d, p)) {
    if (d->type != XML_NODE)
      continue;
    s = xml_node_name(d->value, p);
    if (!s)
      continue;
    if (!strcmp(s, "starttls"))
      ret |= XMPP_FEATURE_STARTTLS;
    else if (!strcmp(s, "bind"))
      ret |= XMPP_FEATURE_BIND;
    else if (!strcmp(s, "session"))
      ret |= XMPP_FEATURE_SESSION;
    else if (!strcmp(s, "mechanisms"))
      ret |= xmpp_sasl_mechanisms(d->value, p);
  }
  return ret;
}

int
xmpp_starttls(struct xmpp *xmpp)
{
  const char *s = "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>";
  return xmpp->io->send(strlen(s), s, xmpp->io);
}

int
xmpp_stream_hook(int node, struct xmpp *xmpp)
{
  return 0;
}

static char *
get_digest(char *msg, const char *search, char end)
{
  char *r, *t, p;
  r = strstr(msg, search);
  if (r) {
    r += strlen(search);
    for (p = 0, t = r; *t && (*t != end || p == '\\'); p = *t, t++) {}
    if (!*t)
      return 0;
    *t = 0;
  }
  return r;
}

static int
hex_from_bin(int dst_bytes, char *dst, int src_bytes, unsigned char *src)
{
  if (dst_bytes < src_bytes * 2 + 1)
    return -1;
  for (;src_bytes; --src_bytes, ++src, dst += 2)
    sprintf(dst, "%02x", (unsigned char)*src);
  *dst = 0;
  return 0;
}

static void
sasl_md5(char md5sum[33], char *realm, char *nonce, char *cnonce, 
         struct xmpp *xmpp)
{
  md5_state_t md5;
  unsigned char a1_h[16], a1[16], a2[16], ret[16];
  char a1s[33], a2s[33], rets[33];
  const char auth[] = "AUTHENTICATE:xmpp/";

  md5_init(&md5);
  md5_append(&md5, xmpp->auth_user, strlen(xmpp->auth_user));
  md5_append(&md5, ":", 1);
  md5_append(&md5, realm, strlen(realm));
  md5_append(&md5, ":", 1);
  md5_append(&md5, xmpp->auth_pwd, strlen(xmpp->auth_pwd));
  md5_finish(&md5, a1_h);

  md5_init(&md5);
  md5_append(&md5, a1_h, 16);
  md5_append(&md5, ":", 1);
  md5_append(&md5, nonce, strlen(nonce));
  md5_append(&md5, ":", 1);
  md5_append(&md5, cnonce, strlen(cnonce));
  md5_finish(&md5, a1);
  hex_from_bin(sizeof(a1s), a1s, sizeof(a1), a1);

  md5_init(&md5);
  md5_append(&md5, auth, strlen(auth));
  md5_append(&md5, xmpp->server, strlen(xmpp->server));
  md5_finish(&md5, a2);
  hex_from_bin(sizeof(a2s), a2s, sizeof(a2), a2);

  md5_init(&md5);
  md5_append(&md5, a1s, 32);
  md5_append(&md5, ":", 1);
  md5_append(&md5, nonce, strlen(nonce));
  md5_append(&md5, ":00000001:", 10);
  md5_append(&md5, cnonce, strlen(cnonce));
  md5_append(&md5, ":auth:", 6);
  md5_append(&md5, a2s, 32);
  md5_finish(&md5, ret);
  hex_from_bin(sizeof(md5sum), md5sum, sizeof(ret), ret);
}

int
xmpp_maks_sasl_response(char *msg, struct xmpp *xmpp)
{
  char *realm, *nonce, *resp;
  char cnonce[SASL_CNONCE_LEN * 8 + 1], md5sum[33];
  int i, t, d, len, slen;

  realm = get_digest(msg, "realm=\"", '"');
  nonce = get_digest(msg, "nonce=\"", '"');

  if (!nonce)
    return POOL_NIL;
  if (!realm)
    realm = xmpp->server;

  for (i = 0; i < SASL_CNONCE_LEN; i++)
    sprintf(cnonce + i * 8, "%08x", rand());

  sasl_md5(md5sum, realm, nonce, cnonce, xmpp);

  t = xml_printf(&xmpp->mem, POOL_NIL,
                 "username=\"%S\",realm=\"%S\",nonce=\"%S\",cnonce=\"%S\","
                 "nc=00000001,qop=auth,digest-uri=\"xmpp/%S\",response=%S,"
                 "charset=utf-8",
                 xmpp->auth_user, realm, nonce, cnonce, xmpp->server, md5sum);
  resp = pool_ptr(&xmpp->mem, t);
  if (!resp)
    return POOL_NIL;
  slen = strlen(resp);
  len = base64_enclen(slen) + 1;
  d = pool_new(&xmpp->mem, len);
  base64_encode(len, pool_ptr(&xmpp->mem, d), slen, resp);

  x = xml_new("response");
  if (x == POOL_NIL || xml_node_add_text_id(x, d, &xmpp->mem))
    return POOL_NIL;
  return x;
}

int
xmpp_sasl_challenge(int node, struct xmpp *xmpp)
{
  char *text;
  int slen, len, n, m, mark, ret;

  text = xml_node_text(node, &xmpp->xml.mem);
  if (!text)
    return -1;
  slen = strlen(text);
  len = base64_declen(slen);
  mark = pool_state(&xmpp->mem);
  m = pool_new(&xmpp->mem, len + 1);
  if (!base64_decode(len, pool_ptr(&xmpp->mem, m), slen, text)) {
    pool_restore(&xmpp->mem, mark);
    return -1;
  }
  text = pool_ptr(&xmpp->mem, m);
  if (!text) {
    pool_restore(&xmpp->mem, mark);
    return -1;
  }
  if (strstr(text, "rspauth"))
    n = xml_new("response", &xmpp->mem);
  else
    n = xmpp_make_sasl_reponse(text, xmpp);
  if (n == POOL_NIL
      || xml_node_add_attr(n, "xmlns", xmpp_ns_sasl, &xmpp->mem)
      || xmpp_send_node(n, xmpp))) {
    pool_restore(&xmpp->mem, mark);
    return -1;
  }
  pool_restore(&xmpp->mem, mark);
  return 0;
}

int
xmpp_default_node_hook(int node, struct xmpp *xmpp)
{
  char *id;
  id = xml_node_name(node, &xmpp->xml.mem);
  if (!id)
    return -1;
  switch(xmpp->state) {
  case XMPP_STATE_TLS:
    if (!strcmp(id, "proceed")) {
      /* todo start tls on IO */
      return 1;
    } else if (!strcmp(id, "failure"))
      return -1;
    break;
  default: 
    if (!strcmp(id, "challenge"))
      return xmpp_sasl_challenge(node, xmpp);
  }
  return 0;
}
