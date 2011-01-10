#define XMPP_BUF_BYTES 256

#define XMPP_SASL_PLAIN 1
#define XMPP_SASL_MD5 2
#define XMPP_FEATURE_STARTTLS 4
#define XMPP_FEATURE_BIND 8
#define XMPP_FEATURE_SESSION 16

enum xmpp_state {
  XMPP_STATE_ERROR = -1,
  XMPP_STATE_NORMAL,
  XMPP_STATE_TLS,
  XMPP_STATE_SASL,
};

struct io {
  int (*send)(int bytes, char *buf, void *user);
  int (*recv)(int bytes, char *buf, void *user);
};

struct xmpp {
  struct xml xml;
  struct pool mem;
  struct jid jid;
  struct io *io;
  enum xmpp_state state;
  char server[XMPP_BUF_BYTES];
  char auth_user[XMPP_BUF_BYTES];
  char auth_pwd[XMPP_BUF_BYTES];

  int (*stream_fn)(int node, void *user);
  int (*node_fn)(int node, void *user);
  int (*error_fn)(char *msg, void *user);
};

struct jid {
  char *full;
  char *partial;
  char *name;
  char *server;
  char *resource;
};

int xmpp_process_input(int bytes, const char *buf, struct xmpp *xmpp, 
                       void *user);
int xmpp_send_node(int node, struct xmpp *xmpp);
int xmpp_default_node_hook(int node, struct xmpp *xmpp);
