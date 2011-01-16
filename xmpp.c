#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pool.h"
#include "node.h"
#include "xml.h"
#include "xml_states.h"
#include "xmpp.h"
#include "base64.h"
#include "md5.h"

#define SASL_CNONCE_LEN 4

static const char xmpp_ns_sasl[] = "urn:ietf:params:xml:ns:xmpp-sasl";
static const char xmpp_ns_bind[] = "urn:ietf:params:xml:ns:xmpp-bind";
static const char xmpp_ns_session[] = "urn:ietf:params:xml:ns:xmpp-session";
static const char xmpp_head_fmt[] =
    "<?xml version='1.0'?><stream:stream "
    "xmlns:stream='http://etherx.jabber.org/streams' xmlns='jabber:client' "
    "to='%s' version='1.0'>";

int xmpp_authorize(struct xmpp *xmpp);
int xmpp_resource_bind(struct xmpp *xmpp);
int xmpp_start_session(struct xmpp *xmpp);

char *xmpp_trim_ws(char *src);
int xmpp_escape_str(int dst_bytes, char *dst, const char *src);

int
xmpp_init(struct xmpp *xmpp, int stack_size)
{
  int len;
  char *s;

  if (!xmpp->server[0]) {
    s = jid_server(xmpp->jid, &len);
    if (!s || !len || len > sizeof(xmpp->server) - 1)
      return -1;
    memcpy(xmpp->server, s, len);
    xmpp->server[len] = 0;
  }
  if (!xmpp->user[0]) {
    s = jid_name(xmpp->jid, &len);
    if (!s || !len || len > sizeof(xmpp->user) - 1)
      return -1;
    memcpy(xmpp->user, s, len);
    xmpp->user[len] = 0;
  }
  xmpp->mem.dsize = xmpp->xml.mem.dsize = xmpp->xml.stack.dsize = stack_size;
  return xml_init(&xmpp->xml);
}

void
xmpp_clean(struct xmpp *xmpp)
{
  xml_clean(&xmpp->xml);
  pool_clean(&xmpp->mem);
}

int
xmpp_process_input(int bytes, const char *buf, struct xmpp *xmpp, void *user)
{
  int i;
  char *id;

  for (i = 0; i < bytes; i++) {
    if (xmpp->xml.level <= 1
        && (xmpp->xml.state == XML_STATE_CONTENTS
            || xmpp->xml.state == XML_STATE_0)
        && isspace(buf[i]))
      continue;

    if (xml_next_char(buf[i], &xmpp->xml))
      return -1;

    switch (xmpp->xml.state) {
    case XML_STATE_NODE_HEAD:
      if (xmpp->xml.level != 1)
        break;
      id = pool_ptr(&xmpp->xml.mem, xmpp->xml.node_id);
      if (!id)
        return -1;
      if (!strcmp("stream:stream", id) && xmpp->stream_fn
          && xmpp->stream_fn(xmpp->xml.node, user))
        return -1;
      xmpp->xml_mem_state = pool_state(&xmpp->xml.mem);
      break;

    case XML_STATE_NODE_END:
      if (!xmpp->xml.level)
        pool_clean(&xmpp->xml.mem);
      if (xmpp->xml.level != 1)
        break;
      if (xmpp->xml.last_node != POOL_NIL && xmpp->node_fn
          && xmpp->node_fn(xmpp->xml.last_node, user)) {
        pool_restore(&xmpp->xml.mem, xmpp->xml_mem_state);
        return -1;
      }
      break;
    }
  }
  return 0;
}

int
xmpp_send_node(int node, struct xmpp *xmpp)
{
  int mark, bytes = 0, ret = -1;
  char *s;

  mark = pool_state(&xmpp->mem);
  s = str_from_xml_node(&xmpp->mem, node, &xmpp->mem);
  if (s) {
    bytes = pool_state(&xmpp->mem) - mark - 1;
    ret = xmpp->send(bytes, s, xmpp->io_context);
  }
  pool_restore(&xmpp->mem, mark);
  return (ret == bytes) ? 0 : -1;
}

int
xmpp_start(struct xmpp *xmpp)
{
  int mark, h, n, ret = -1;
  char *s;

  xml_reset(&xmpp->xml);
  mark = pool_state(&xmpp->mem);
  h = xml_printf(&xmpp->mem, POOL_NIL, xmpp_head_fmt, xmpp->server);
  s = pool_ptr(&xmpp->mem, h);
  n = strlen(s);
  if (s && (xmpp->send(n, s, xmpp->io_context) == n))
    ret = 0;
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

  s = xml_node_name(x, p);
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
  int n = strlen(s);
  xmpp->state = XMPP_STATE_TRY_TLS;
  return (xmpp->send(n, s, xmpp->io_context) == n) ? 0 : -1;
}

static char *
get_digest(char *msg, const char *search, char till, char **end)
{
  char *r, *t, p;
  r = strstr(msg, search);
  *end = 0;
  if (r) {
    r += strlen(search);
    for (p = 0, t = r; *t && (*t != till || p == '\\'); p = *t, t++) {}
    if (!*t)
      return 0;
    *end = t;
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

static int
sasl_md5(char md5sum[33], char *realm, char *nonce, char *cnonce,
         char *user, char *pwd, char *server)
{
  md5_state_t md5;
  unsigned char a1_h[16], a1[16], a2[16], ret[16];
  char a1s[33], a2s[33];
  const char auth[] = "AUTHENTICATE:xmpp/";

  ae_md5_init(&md5);
  ae_md5_append(&md5, (md5_byte_t *)user, strlen(user));
  ae_md5_append(&md5, (md5_byte_t *)":", 1);
  ae_md5_append(&md5, (md5_byte_t *)realm, strlen(realm));
  ae_md5_append(&md5, (md5_byte_t *)":", 1);
  ae_md5_append(&md5, (md5_byte_t *)pwd, strlen(pwd));
  ae_md5_finish(&md5, a1_h);

  ae_md5_init(&md5);
  ae_md5_append(&md5, (md5_byte_t *)a1_h, 16);
  ae_md5_append(&md5, (md5_byte_t *)":", 1);
  ae_md5_append(&md5, (md5_byte_t *)nonce, strlen(nonce));
  ae_md5_append(&md5, (md5_byte_t *)":", 1);
  ae_md5_append(&md5, (md5_byte_t *)cnonce, strlen(cnonce));
  ae_md5_finish(&md5, a1);
  hex_from_bin(sizeof(a1s), a1s, sizeof(a1), a1);

  ae_md5_init(&md5);
  ae_md5_append(&md5, (md5_byte_t *)auth, strlen(auth));
  ae_md5_append(&md5, (md5_byte_t *)server, strlen(server));
  ae_md5_finish(&md5, a2);
  hex_from_bin(sizeof(a2s), a2s, sizeof(a2), a2);

  ae_md5_init(&md5);
  ae_md5_append(&md5, (md5_byte_t *)a1s, 32);
  ae_md5_append(&md5, (md5_byte_t *)":", 1);
  ae_md5_append(&md5, (md5_byte_t *)nonce, strlen(nonce));
  ae_md5_append(&md5, (md5_byte_t *)":00000001:", 10);
  ae_md5_append(&md5, (md5_byte_t *)cnonce, strlen(cnonce));
  ae_md5_append(&md5, (md5_byte_t *)":auth:", 6);
  ae_md5_append(&md5, (md5_byte_t *)a2s, 32);
  ae_md5_finish(&md5, ret);
  hex_from_bin(33, md5sum, sizeof(ret), ret);
  return 0;
}

int
xmpp_make_sasl_response(char *msg, struct xmpp *xmpp)
{
  char *realm, *nonce, *realm_end, *nonce_end, *resp;
  char cnonce[SASL_CNONCE_LEN * 8 + 1], md5sum[33];
  char user[XMPP_BUF_BYTES], pwd[XMPP_BUF_BYTES];
  int i, d, len, slen, x;

  realm = get_digest(msg, "realm=\"", '"', &realm_end);
  nonce = get_digest(msg, "nonce=\"", '"', &nonce_end);

  if (!(nonce && nonce_end))
    return POOL_NIL;
  *nonce_end = 0;
  if (realm) {
    if (realm_end)
      *realm_end = 0;
  } else
    realm = xmpp->server;

  for (i = 0; i < SASL_CNONCE_LEN; i++)
    sprintf(cnonce + i * 8, "%08x", rand());

  if (xmpp_escape_str(sizeof(user), user, xmpp->user)
      || xmpp_escape_str(sizeof(pwd), pwd, xmpp->pwd))
    return POOL_NIL;

  if (sasl_md5(md5sum, realm, nonce, cnonce, user, pwd, xmpp->server))
    return POOL_NIL;

  resp = xml_sprintf(&xmpp->mem, POOL_NIL,
                     "username=\"%S\",realm=\"%S\",nonce=\"%S\","
                     "cnonce=\"%S\",nc=00000001,qop=auth,"
                     "digest-uri=\"xmpp/%S\",response=%S,charset=utf-8",
                     user, realm, nonce, cnonce, xmpp->server, md5sum);
  if (!resp)
    return POOL_NIL;
  slen = strlen(resp);
  len = base64_enclen(slen) + 1;
  d = pool_new(&xmpp->mem, len);
  base64_encode(len, pool_ptr(&xmpp->mem, d), slen, resp);

  x = xml_new("response", &xmpp->mem);
  if (x == POOL_NIL || xml_node_add_text_id(x, d, &xmpp->mem))
    return POOL_NIL;
  return x;
}

int
xmpp_sasl_challenge(int node, struct xmpp *xmpp)
{
  char *text;
  int slen, len, n, m, mark;

  text = xml_node_text(node, &xmpp->xml.mem);
  if (!text)
    return -1;
  text = xmpp_trim_ws(text);
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
    n = xmpp_make_sasl_response(text, xmpp);
  if (n == POOL_NIL
      || xml_node_add_attr(n, "xmlns", xmpp_ns_sasl, &xmpp->mem)
      || xmpp_send_node(n, xmpp)) {
    pool_restore(&xmpp->mem, mark);
    return -1;
  }
  pool_restore(&xmpp->mem, mark);
  return 0;
}

int
xmpp_default_node_hook(int node, struct xmpp *xmpp, void *user)
{
  char *id;

  id = xml_node_name(node, &xmpp->xml.mem);
  if (!id)
    return -1;
  if (xmpp->state == XMPP_STATE_TRY_TLS) {
    if (!strcmp(id, "proceed")) {
      if (xmpp->tls_fn && xmpp->tls_fn(user))
        return -1;
      xmpp->state = XMPP_STATE_TLS;
      return (xmpp_start(xmpp) == 0) ? 1 : -1;
    } else if (!strcmp(id, "failure"))
      return -1;
  }
  if (!strcmp(id, "stream:features")) {
    if (xmpp->tls_fn && xmpp->state != XMPP_STATE_TLS)
      return 1;
    xmpp->features = xmpp_stream_features(node, &xmpp->xml.mem);
    if (xmpp->is_authorized) {
      if ((xmpp->features & XMPP_FEATURE_BIND) && xmpp_resource_bind(xmpp))
        return -1;
      if ((xmpp->features & XMPP_FEATURE_SESSION)
          && xmpp_start_session(xmpp))
        return -1;
      if (xmpp->auth_fn && xmpp->auth_fn(node, user))
        return -1;
    } else if (xmpp->use_sasl)
      return (xmpp_authorize(xmpp) == 0) ? 1 : -1;
    return 1;
  } else if (!strcmp(id, "challenge")) {
    return (xmpp_sasl_challenge(node, xmpp) == 0) ? 1 : -1;
  } else if (!strcmp(id, "success")) {
    xmpp->is_authorized = 1;
    return (xmpp_start(xmpp) == 0) ? 1 : -1;
  } else if (!strcmp(id, "failure"))
    return -1;
  return 0;
}

int
xmpp_auth_plain(int node, struct xmpp *xmpp)
{
  int h, slen, len;
  char *s;

  if (xml_node_add_attr(node, "mechanism", "PLAIN", &xmpp->mem))
    return -1;
  h = xml_printf(&xmpp->mem, POOL_NIL, "%C%S%C%S", 0, xmpp->user, 0,
                 xmpp->pwd);
  s = pool_ptr(&xmpp->mem, h);
  if (!s)
    return -1;
  slen = strlen(s);
  len = base64_enclen(slen);
  h = pool_new(&xmpp->mem, len + 1);
  if (!base64_encode(len, pool_ptr(&xmpp->mem, h), slen, s))
    return -1;
  if (xml_node_add_text_id(node, h, &xmpp->mem))
    return -1;
  return 0;
}

int
xmpp_authorize(struct xmpp *xmpp)
{
  int x, mark, ret;

  ret = -1;
  mark = pool_state(&xmpp->mem);
  do {
    x = xml_new("auth", &xmpp->mem);
    if (x == POOL_NIL ||
        xml_node_add_attr(x, "xmlns", xmpp_ns_sasl, &xmpp->mem))
      break;
    if (xmpp->features & XMPP_SASL_MD5) {
      if (xml_node_add_attr(x, "mechanism", "DIGEST-MD5", &xmpp->mem))
        break;
    } else if (xmpp->features & XMPP_SASL_PLAIN) {
      if (xmpp_auth_plain(x, xmpp))
        break;
    } else
      break;
    if(xmpp_send_node(x, xmpp))
      break;
    ret = 0;
  } while (0);
  pool_restore(&xmpp->mem, mark);
  return ret;
}

int
xmpp_resource_bind(struct xmpp *xmpp)
{
  int x, y, z, mark, len, ret = -1;
  char *res;

  mark = pool_state(&xmpp->mem);
  do {
    x = xml_new("iq", &xmpp->mem);
    if (x == POOL_NIL || xml_node_add_attr(x, "type", "set", &xmpp->mem))
      break;
    y = xml_insert(x, "bind", &xmpp->mem);
    if (y == POOL_NIL 
        || xml_node_add_attr(y, "xmlns", xmpp_ns_bind, &xmpp->mem))
      break;
    res = jid_resource(xmpp->jid, &len);
    if (res && len > 0) {
      z = xml_insert(y, "resource", &xmpp->mem);
      if (z == POOL_NIL || xml_node_add_textn(z, len, res, &xmpp->mem))
        break;
    }
    if (xmpp_send_node(x, xmpp))
      break;
    ret = 0;
  } while (0);
  pool_restore(&xmpp->mem, mark);
  return ret;
}

int
xmpp_start_session(struct xmpp *xmpp)
{
  int x, y, mark, ret = -1;
  mark = pool_state(&xmpp->mem);
  do {
    x = xml_new("iq", &xmpp->mem);
    if (x == POOL_NIL || xml_node_add_attr(x, "type", "set", &xmpp->mem)
        || xml_node_add_attr(x, "id", "auth", &xmpp->mem))
      break;
    y = xml_insert(x, "session", &xmpp->mem);
    if (y == POOL_NIL
        || xml_node_add_attr(y, "xmlns", xmpp_ns_session, &xmpp->mem)
        || xmpp_send_node(x, xmpp))
      break;
    ret = 0;
  } while (0);
  pool_restore(&xmpp->mem, mark);
  return ret;
}

char *
xmpp_trim_ws(char *src)
{
  char *end;
  for (; *src && isspace(*src); src++) {}

  end = src + strlen(src);
  for (; *end && isspace(*end); end--)
    *end = 0;
  return src;
}

int
xmpp_escape_str(int dst_bytes, char *dst, const char *src)
{
  for (; *src && dst_bytes > 1; src++, dst++, dst_bytes--) {
    if (*src == '"' || *src == '\\') {
      *dst++ = '\\';
      dst_bytes--;
      if (dst_bytes <= 1)
        break;
    }
    *dst = *src;
  }
  *dst = 0;
  return (dst_bytes > 1) ? 0 : -1;
}

char *
jid_name(char *jid, int *len)
{
  int n;

  for (n = 0; jid[n] && jid[n] != '@'; n++) {}
  *len = n;
  return jid;
}

char *
jid_partial(char *jid, int *len)
{
  int n;

  for (n = 0; jid[n] && jid[n] != '/'; n++) {}
  *len = n;
  return jid;
}

char *
jid_server(char *jid, int *len)
{
  int n;

  for (n = 0; jid[n] && jid[n] != '@'; n++) {}
  if (!jid[n])
    return 0;
  jid += n + 1;
  for (n = 0; jid[n] && jid[n] != '/'; n++) {}
  *len = n;
  return jid;
}

char *
jid_resource(char *jid, int *len)
{
  int n;

  for (n = 0; jid[n] && jid[n] != '/'; n++) {}
  if (!jid[n])
    return 0;
  jid += n + 1;
  *len = strlen(jid);
  return jid;
}
