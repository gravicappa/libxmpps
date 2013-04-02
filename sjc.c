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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <polarssl/ssl.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/entropy.h>

#include "pool.h"
#include "node.h"
#include "xml.h"
#include "xmpp.h"
#include "tls.h"

#define BUF_BYTES 256

static int status = 0;
static int use_tls = 1;
static int use_sasl = 1;
static int show_log = 0;
static int keep_alive_ms = 50000;
static char status_msg[2][BUF_BYTES] = {"", "Away."};
static char jid_to[BUF_BYTES] = "";
static char jid_from[BUF_BYTES] = "";
static char *msg_type = "chat";

static char *x_roster = "<iq type='get' id='roster'>"
                        "<query xmlns='jabber:iq:roster'/></iq>";

static struct tls tls;
static int in_tls = 0;

static int
tcp_connect(char *host, int port)
{
  struct sockaddr_in srv_addr;
  struct hostent *srv_host;
  int fd;

  srv_host = gethostbyname(host);
  if (!srv_host || sizeof(srv_addr.sin_addr) < srv_host->h_length)
    return -1;
  fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (fd < 0)
    return -1;
  memcpy(&srv_addr.sin_addr, *srv_host->h_addr_list, srv_host->h_length);
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons(port);
  if (connect(fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static int
tcp_recv(int bytes, char *buf, void *user)
{
  int n;
  do {
    n = recv(*(int *)user, buf, bytes, 0);
  } while (n < 0 && errno == EAGAIN);
  if (show_log)
    fprintf(stderr, "tcp_recv [%d/%d]\n", n, bytes);
  if (bytes > 0 && !n)
    return -1;
  return n;
}

static int
tcp_send(int bytes, const char *buf, void *user)
{
  int w, n;
  n = 0;
  while (n < bytes) {
    w = send(*(int *)user, buf + n, bytes - n, 0);
    if (w < 0)
      return -1;
    n += w;
  }
  if (show_log)
    fprintf(stderr, "tcp_send [%d/%d]\n", n, bytes);
  return n;
}

static int
io_recv(int bytes, char *buf, int *remain, void *user)
{
  int n;
  *remain = 0;
  n = (in_tls)
      ? tls_recv(bytes, buf, remain, &tls) : tcp_recv(bytes, buf, user);
  if (n > 0 && show_log)
    fprintf(stderr, "\n<-%c[%d] '%.*s'\n\n", (in_tls) ? '&' : ' ', n, n, buf);
  return n;
}

static int
io_send(int bytes, const char *buf, void *user)
{
  int i;
  if (show_log)
    for (i = 0; i < bytes; i++)
      if (!isspace(buf[i])) {
        fprintf(stderr, "\n->%c[%d] %.*s\n\n", (in_tls) ? '&' : ' ', bytes,
                bytes, buf);
        break;
      }
  return (in_tls) ? tls_send(bytes, buf, &tls) : tcp_send(bytes, buf, user);
}

static int
auth_handler(int x, void *user)
{
  struct xmpp *xmpp = (struct xmpp *)user;
  xmpp_printf(xmpp, "<presence><show/></presence>");
  xmpp_printf(xmpp, x_roster);
  return 0;
}

static int
start_tls(void *user)
{
  struct xmpp *xmpp = (struct xmpp *)user;
  if (!in_tls) {
    memset(&tls, 0, sizeof(tls));
    tls.user = xmpp->io_context;
    tls.recv = tcp_recv;
    tls.send = tcp_send;
    if (tls_start(&tls)) {
      fprintf(stderr, "err: TLS start failure\n");
      return -1;
    }
    in_tls = 1;
  }
  return 0;
}

static int
stream_handler(int x, void *user)
{
  struct xmpp *xmpp = (struct xmpp *)user;
  if (!in_tls && use_tls)
    if (xmpp_starttls(xmpp))
      return -1;
  return 0;
}

static void
print_msg(const char *fmt, ...)
{
  char date[BUF_BYTES];
  time_t t;
  va_list args;
  va_start(args, fmt);
  t = time(0);
  strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&t));
  printf("%s ", date);
  vprintf(fmt, args);
  va_end(args);
}

static void
roster_handler(int x, struct xmpp *xmpp)
{
  struct xml_data *d;
  char *jid, *name, *sub;
  for (d = xml_node_data(xml_node_find(x, "query", &xmpp->xml.mem),
                         &xmpp->xml.mem);
       d;
       d = xml_data_next(d, &xmpp->xml.mem)) {
    if (d->type != XML_NODE)
      continue;
    jid = xml_node_find_attr(d->value, "jid", &xmpp->xml.mem);
    name = xml_node_find_attr(d->value, "name", &xmpp->xml.mem);
    sub = xml_node_find_attr(d->value, "subscription", &xmpp->xml.mem);
    print_msg("* %s - %s - [%s]\n", name ? name : "", jid, sub);
  }
  print_msg("End of roster\n");
  for (d = xml_node_data(xml_node_find(x, "query", &xmpp->xml.mem),
                         &xmpp->xml.mem);
       d;
       d = xml_data_next(d, &xmpp->xml.mem)) {
    if (d->type != XML_NODE)
      continue;
    jid = xml_node_find_attr(d->value, "jid", &xmpp->xml.mem);
    if (jid)
      xmpp_printf(xmpp, "<presence type='probe' to='%s'/>", jid);
  }
}

static int
node_handler(int x, void *user)
{
  struct xmpp *xmpp = (struct xmpp *)user;
  char *name, *msg, *from, *show, *status, *type;
  int r, n;

  r = xmpp_default_node_hook(x, xmpp, user);
  if (r < 0)
    return -1;
  if (r)
    return 0;

  name = xml_node_name(x, &xmpp->xml.mem);
  if (!name)
    return -1;
  from = xml_node_find_attr(x, "from", &xmpp->xml.mem);
  if (!strcmp(name, "message")) {
    from = from ? jid_partial(from, &n) : 0;
    if (!from)
      return -1;
    snprintf(jid_from, sizeof(jid_from), "%.*s", n, from);
    msg = xml_node_find_text(x, "body", &xmpp->xml.mem);
    if (!msg)
      return -1;
    print_msg("<%.*s> %s\n", n, from, msg);
  } else if (!strcmp(name, "presence")) {
    if (!from)
      return -1;
    show = xml_node_find_text(x, "show", &xmpp->xml.mem);
    status = xml_node_find_text(x, "status", &xmpp->xml.mem);
    type = xml_node_find_attr(x, "type", &xmpp->xml.mem);
    if (type)
      print_msg("-!- %s sends %s\n", from, type);
    print_msg("-!- %s is %s (%s)\n", from,
              show ? show : (type ? type : "online"), status ? status : "");
  } else if (!strcmp(name, "iq")) {
    name = xml_node_find_attr(x, "id", &xmpp->xml.mem);
    if (name && !strcmp(name, "roster"))
      roster_handler(x, xmpp);
  }
  return 0;
}

static int
process_server_input(int fd, struct xmpp *xmpp)
{
  char buf[BUF_BYTES];
  int n, remain;

  do {
    n = io_recv(sizeof(buf), buf, &remain, &fd);
    if (n <= 0)
      return -1;
    if (xmpp_process_input(n, buf, xmpp, xmpp))
      return -1;
  } while (remain > 0);
  return 0;
}

static int
read_line(int fd, size_t len, char *buf)
{
  char c = 0;
  size_t i = 0;

  do {
    if (read(fd, &c, sizeof(char)) != sizeof(char))
      return -1;
    buf[i++] = c;
  } while (c != '\n' && i < len);
  buf[i - 1] = 0;
  return 0;
}

static int
process_input(int fd, struct xmpp *xmpp)
{
  char buf[BUF_BYTES];
  char *s;
  char *pres = "<presence><show>%s</show><status>%s</status></presence>";
  char *epres = "<presence to='%s' type='%s'/>";
  char *jpres = "<presence to='%s'>"
                "<x xmlns='http://jabber.org/protocol/muc'/></presence>";
  char *msg = "<message to='%s' type='%s'><body>%s</body></message>";
  static const char *show[2] = { "online", "away" };

  if (read_line(fd, sizeof(buf), buf))
    return -1;

  if (buf[0] != ':')
    xmpp_printf(xmpp, msg, jid_to, msg_type, buf);
  else {
    switch (buf[1]) {
    case 'a':
      status = !status;
      if (buf[2])
        snprintf(status_msg[status], sizeof(status_msg[status]), "%s",
                 buf + 3);
      xmpp_printf(xmpp, pres, show[status], status_msg[status]);
      break;
    case 'g':
      msg_type = "groupchat";
    case 'm':
      if (buf[1] != 'g')
        msg_type = "chat";
      s = strchr(buf + 3, ' ');
      if (!s)
        break;
      *s++ = 0;
      snprintf(jid_to, sizeof(jid_to), "%s", buf + 3);
      xmpp_printf(xmpp, msg, jid_to, msg_type, s);
      break;
    case 'r':
      memcpy(jid_to, jid_from, sizeof(jid_to));
      if (!buf[2])
        break;
      xmpp_printf(xmpp, msg, jid_to, buf + 3);
      break;
    case 'j': xmpp_printf(xmpp, jpres, buf + 3); break;
    case 'l': xmpp_printf(xmpp, epres, buf + 3, "unavailable"); break;
    case 'w': xmpp_printf(xmpp, x_roster); break;
    case '<': xmpp_printf(xmpp, "%S", buf + 3); break;
    default:
      if (jid_to[0])
        xmpp_printf(xmpp, msg, jid_to, msg_type, buf);
    }
  }
  return 0;
}

static int
process_connection(int fd, struct xmpp *xmpp)
{
  struct timeval tv;
  fd_set fds;
  int max_fd, ret = 0;

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  while (1) {
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    FD_SET(0, &fds);
    max_fd = fd;
    tv.tv_sec = keep_alive_ms / 1000;
    tv.tv_usec = (keep_alive_ms % 1000) * 1000;
    ret = select(max_fd + 1, &fds, 0, 0, (keep_alive_ms > 0) ? &tv : 0);
    if (ret < 0)
      break;
    if (ret > 0) {
      if (FD_ISSET(fd, &fds))
        if (process_server_input(fd, xmpp))
          break;
      if (FD_ISSET(0, &fds))
        if (process_input(0, xmpp))
          break;
    } else if ((!use_tls || in_tls) && io_send(1, " ", &fd) < 1)
      break;
  }
  return 0;
}

static void
die_usage(void)
{
  fprintf(stderr, "%s",
          "sjc - simple jabber client - " VERSION "\n"
          "(C)opyright 2010-2011 Ramil Farkhshatov\n"
          "usage: sjc [-j jid] [-s server] [-p port] [-k pwdfile]\n");
  exit(1);
}

static int
read_pw(const char *filename, struct xmpp *xmpp)
{
  int fd, ret = -1;

  if (!filename)
    return -1;
  fd = open(filename, O_RDONLY);
  if (fd < 0)
    return -1;
  if(!read_line(fd, sizeof(xmpp->pwd), xmpp->pwd))
    ret = 0;
  close(fd);
  return ret;
}

int
main(int argc, char **argv)
{
  struct xmpp xmpp = { 0 };
  int i, port = XMPP_PORT, fd, ret = 1;
  char *jid = 0, *pwdfile = 0, *srv = 0;

  for (i = 1; i < argc - 1 && argv[i][0] == '-'; i++)
    switch (argv[i][1]) {
    case 'j': jid = argv[++i]; break;
    case 'k': pwdfile = argv[++i]; break;
    case 's': srv = argv[++i]; break;
    case 'l': show_log = atoi(argv[++i]); break;
    case 'p': port = atoi(argv[++i]); break;
    default: die_usage();
    }
  if (!jid)
    die_usage();

  xmpp.io_context = &fd;
  xmpp.send = io_send;
  xmpp.tls_fn = use_tls ? start_tls : 0;
  xmpp.stream_fn = stream_handler;
  xmpp.node_fn = node_handler;
  xmpp.auth_fn = auth_handler;
  xmpp.use_sasl = use_sasl;
  xmpp.jid = jid;

  read_pw(pwdfile, &xmpp);

  if (xmpp_init(&xmpp, 4096))
    return 1;

  if (!srv)
    srv = xmpp.server;

  fd = tcp_connect(srv, port);
  if (fd < 0)
    return 1;

  if (!(xmpp_start(&xmpp) || process_connection(fd, &xmpp)))
    ret = 0;

  xmpp_printf(&xmpp, "</stream:stream>");
  xmpp_clean(&xmpp);
  close(fd);
  shutdown(fd, 2);
  return ret;
}
