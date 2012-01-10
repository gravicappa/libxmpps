struct tls {
  void *user;
  int (*recv)(int bytes, char *buf, void *user);
  int (*send)(int bytes, const char *buf, void *user);
  ctr_drbg_context ctr_drbg;
  entropy_context entropy;
  ssl_context ssl;
  ssl_session ssn;
};

int tls_start(struct tls *tls);
void tls_cleanup(struct tls *tls);
int tls_recv(int len, char *buf, int *remain, void *user);
int tls_send(int len, const char *buf, void *user);
