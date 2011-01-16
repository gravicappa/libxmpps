#define XMPP_BUF_BYTES 256
#define XMPP_PORT 5222

#define XMPP_SASL_PLAIN 1
#define XMPP_SASL_MD5 2
#define XMPP_FEATURE_STARTTLS 4
#define XMPP_FEATURE_BIND 8
#define XMPP_FEATURE_SESSION 16

enum xmpp_state {
  XMPP_STATE_ERROR = -1,
  XMPP_STATE_NORMAL,
  XMPP_STATE_TRY_TLS,
  XMPP_STATE_TLS
};

struct xmpp {
  void *io_context;
  int (*send)(int bytes, const char *buf, void *user);

  struct xml xml;
  struct pool mem;

  enum xmpp_state state;

  int use_sasl;
  int use_plain;

  int features;
  int is_authorized;
  int is_ready;
  int xml_mem_state;

  char *jid;
  char server[XMPP_BUF_BYTES];
  char user[XMPP_BUF_BYTES];
  char pwd[XMPP_BUF_BYTES];

  int (*tls_fn)(void *user);
  int (*stream_fn)(int node, void *user);
  int (*auth_fn)(int node, void *user);
  int (*node_fn)(int node, void *user);
  int (*error_fn)(char *msg, void *user);
  void (*log_fn)(int dir, char *msg, int node, void *user);
};

int xmpp_init(struct xmpp *xmpp, int stack_size);
void xmpp_clean(struct xmpp *xmpp);
int xmpp_process_input(int bytes, const char *buf, struct xmpp *xmpp, 
                       void *user);
int xmpp_send_node(int node, struct xmpp *xmpp);
int xmpp_printf(struct xmpp *xmpp, const char *fmt, ...);
int xmpp_default_node_hook(int node, struct xmpp *xmpp, void *user);
int xmpp_start(struct xmpp *xmpp);
int xmpp_starttls(struct xmpp *xmpp);

char *jid_name(char *jid, int *len);
char *jid_partial(char *jid, int *len);
char *jid_server(char *jid, int *len);
char *jid_resource(char *jid, int *len);
