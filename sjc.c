#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#ifdef USE_TLS
#include <polarssl/ssl.h>
#include <polarssl/havege.h>
#endif/*USE_TLS*/

#include "pool.h"
#include "node.h"
#include "xml.h"
#include "xmpp.h"

#ifdef USE_TLS
#include "tls.h"
#endif/*USE_TLS*/

#define BUF_BYTES 512

static int fd = -1;
static int status = 0;
static int use_tls = 1;
static int show_log = 1;
static char status_msg[2][BUF_BYTES] = {"", "Away."};
static char to[BUF_BYTES] = "";
static char from[BUF_BYTES] = "";

static struct tls tls;
static int in_tls = 0;

int
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
  memcpy(&srv_addr.sin_addr, srv_host->h_addr, srv_host->h_length);
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons(port);
  if (connect(fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

int
net_blocking(int fd, int blocking)
{
  if (blocking)
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
  else 
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static int
tcp_recv(int bytes, char *buf, void *user)
{
  int n; 
  do {
    n = recv(fd, buf, bytes, 0);
  } while (n < 0 && errno == EAGAIN);
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
    w = send(fd, buf + n, bytes - n, 0);
    if (w < 0)
      return -1;
    n += w;
  }
  fprintf(stderr, "tcp_send [%d/%d]\n", n, bytes);
  return n;
}

static int
io_recv(int bytes, char *buf, void *user)
{
  int n;
  n = (in_tls) ? tls_recv(bytes, buf, &tls) : tcp_recv(bytes, buf, user);
  if (n > 0 && show_log) {
    fprintf(stderr, "\n<- %c[%d] '", (in_tls) ? '&' : ' ', n);
    fwrite(buf, 1, n, stderr);
    fprintf(stderr, "'\n\n");
  }
  return n;
}

static int
io_send(int bytes, const char *buf, void *user)
{
  int i;
  if (show_log)
    for (i = 0; i < bytes; i++)
      if (!isspace(buf[i])) {
        fprintf(stderr, "\n-> %c[%d] ", (in_tls) ? '&' : ' ', bytes);
        fwrite(buf, 1, bytes, stderr);
        fprintf(stderr, "\n\n");
        break;
      }

  return (in_tls) ? tls_send(bytes, buf, &tls) : tcp_send(bytes, buf, user);
}

static char *
node_from(int x, int *len, struct xmpp *xmpp)
{
  char *from;
  from = xml_node_find_attr(x, "from", &xmpp->xml.mem);
  return from ? jid_partial(from, len) : 0;
}

static int
auth_handler(int x, void *user)
{
  char *p = "<presence><show/></presence>";
  io_send(strlen(p), p, &fd);
  return 0;
}

static int
start_tls(void *user)
{
  if (!in_tls) {
    memset(&tls, 0, sizeof(tls));
    tls.recv = tcp_recv;
    tls.send = tcp_send;
    fprintf(stderr, "tls starting\n");
    /*net_blocking(fd, 1);*/
    if (tls_start(&tls))
      return -1;
    /*net_blocking(fd, 0);*/
    fprintf(stderr, "tls started\n");
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
  fprintf(stderr, "stream handler => %d\n", 0);
  return 0;
}

static int
node_handler(int x, void *user)
{
  struct xmpp *xmpp = (struct xmpp *)user;
  char *name, *msg, *msg_from, *show, *status;
  int r, n, ret = 0;
  char date[BUF_BYTES];
  time_t t;

  r = xmpp_default_node_hook(x, xmpp, user);
  if (r < 0)
    return 1;
  if (r)
    return 0;

  name = xml_node_name(x, &xmpp->xml.mem);
  if (!name)
    return -1;
  if (!strcmp(name, "message")) {
    msg_from = node_from(x, &n, xmpp);
    if (!msg_from)
      return -1;
    snprintf(from, sizeof(from), "%s", msg_from);
    msg = xml_node_text(xml_node_find(x, "body", &xmpp->xml.mem),
                        &xmpp->xml.mem);
    if (!msg)
      return -1;
    t = time(0);
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&t));
    fwrite(date, 1, strlen(date), stdout);
    fwrite(" ", 1, 1, stdout);
    fwrite(msg_from, 1, n, stdout);
    printf(": %s\n", msg);
  } else if (!strcmp(name, "presense")) {
    msg_from = node_from(x, &n, xmpp);
    if (!msg_from)
      return -1;
    show = xml_node_text(xml_node_find(x, "show", &xmpp->xml.mem),
                         &xmpp->xml.mem);
    status = xml_node_text(xml_node_find(x, "status", &xmpp->xml.mem),
                           &xmpp->xml.mem);
    t = time(0);
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&t));
    fwrite(date, 1, strlen(date), stdout);
    fwrite(" ", 1, n, stdout);
    fwrite(msg_from, 1, n, stdout);
    printf(" is %s '%s'\n", show ? show : "online", status ? status : "");
  }
  return ret;
}

int
process_server_input(int fd, struct xmpp *xmpp)
{
  char buf[BUF_BYTES];
  int n;

  n = io_recv(sizeof(buf), buf, &fd);
  if (n <= 0)
    return -1;
  if (xmpp_process_input(n, buf, xmpp, xmpp))
    return -1;
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

int
process_input(int fd, struct xmpp *xmpp)
{
  char buf[BUF_BYTES];
  char *s, *p;
  char *pres = "<presence><show>%s</show><status>%s</status></presence>";
  char *msg = "<message to='%s'><body>%s</body></message>";
  static const char *show[2] = { "online", "away" };
  int mark;

  if (read_line(fd, sizeof(buf), buf))
    return -1;

  p = strchr(buf, ' ');
  if (p)
    *p++ = 0;
  mark = pool_state(&xmpp->mem);
  if (!strcmp(buf, ":a")) {
    status = !status;
    if (p) {
      s = strchr(p, ' ');
      if (s) {
        s++;
        snprintf(status_msg[status], sizeof(status_msg[status]), "%s", s);
      }
    }
    p = pool_ptr(&xmpp->mem,
                 xml_printf(&xmpp->mem, POOL_NIL, pres, show[status],
                            status_msg[status]));
    if (p)
      io_send(strlen(p), p, &fd);
  } else if (!strcmp(buf, ":m")) {
    s = strchr(p, ' ');
    if (s) {
      *s++ = 0;
      snprintf(to, sizeof(to), "%s", p);
    }
    p = pool_ptr(&xmpp->mem, xml_printf(&xmpp->mem, POOL_NIL, msg, to, s));
    if (p)
      io_send(strlen(p), p, &fd);
  } else if (!strcmp(buf, ":r")) {
    memcpy(to, from, sizeof(to));
    if (!p)
      return 0;
    p = pool_ptr(&xmpp->mem, xml_printf(&xmpp->mem, POOL_NIL, msg, to, p));
    if (p)
      io_send(strlen(p), p, &fd);
  } else {
    p = pool_ptr(&xmpp->mem, xml_printf(&xmpp->mem, POOL_NIL, msg, to, buf));
    if (p)
      io_send(strlen(p), p, &fd);
  }
  pool_restore(&xmpp->mem, mark);
  return 0;
}

int
process_connection(int fd, struct xmpp *xmpp)
{
  struct timeval tv;
  fd_set fds;
  int max_fd, ret = 0;
  int keep_alive_ms = 25000;

  net_blocking(fd, 0);
  while (1) {
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    FD_SET(0, &fds);
    max_fd = fd;
    tv.tv_sec = 0;
    tv.tv_usec = keep_alive_ms * 1000;
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
    } /*else if (io_send(1, " ", &fd) < 1)
      break;*/
  }
  return 0;
}

static void
die_usage(void)
{
  fprintf(stderr, "%s",
          "sjc - simple jabber client - " SJC_VERSION "\n"
          "(C)opyright 2010 Ramil Farkhshatov\n"
          "usage: sjc [-j <jid>] [-s <server>] [-p <port>] [-k <pwdfile>]\n");
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
  int i, port = XMPP_PORT, ret = 1;
  char *jid = 0, *pwdfile = 0, *srv = 0;

  for (i = 1; i < argc - 1 && argv[i][0] == '-'; i++)
    switch (argv[i][1]) {
    case 'j': jid = argv[++i]; break;
    case 'k': pwdfile = argv[++i]; break;
    case 's': srv = argv[++i]; break;
    case 'p':
      if (sscanf(argv[++i], "%d", &port) != 1)
        die_usage();
      break;
    default: die_usage();
    }
  if (!jid)
    die_usage();

  xmpp.send = io_send;
  xmpp.tls_fn = start_tls;
  xmpp.stream_fn = stream_handler;
  xmpp.node_fn = node_handler;
  xmpp.auth_fn = auth_handler;
  xmpp.use_sasl = 1;
  xmpp.jid = jid;
#if 0
  if (srv)
    snprintf(xmpp.server, sizeof(xmpp.server), "%s", srv);
#endif

  if (read_pw(pwdfile, &xmpp))
    xmpp.pwd[0] = 0;

  if (xmpp_init(&xmpp, 4096))
    return 1;

  if (!srv)
    srv = xmpp.server;

  fd = tcp_connect(srv, port);
  if (fd < 0)
    return 1;

  if (!(xmpp_start(&xmpp) != 0 || process_connection(fd, &xmpp)))
    ret = 0;

  xmpp_clean(&xmpp);
  close(fd);
  shutdown(fd, 2);
  return ret;
}
