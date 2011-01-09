struct pool {
  int dsize;
  int used;
  int size;
  void *buf;
};

#define POOL_NIL (-1)

int pool_new(struct pool *p, int size);
void pool_clean(struct pool *p);
int pool_new_str(struct pool *p, const char *str);
int pool_new_strn(struct pool *p, const char *str, int len);
char *pool_ptr(struct pool *p, int h);

int pool_state(struct pool *p);
void pool_restore(struct pool *p, int mark);

int pool_append_str(struct pool *p, int h, const char *str);
int pool_append_strn(struct pool *p, int h, const char *str, int len);
int pool_append_char(struct pool *p, int h, char c);
