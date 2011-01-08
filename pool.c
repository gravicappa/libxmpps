#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pool.h"

static struct pool pool = {1024};

int pool_new(struct pool *p, int size);
void pool_clean(struct pool *p);
int pool_new_str(struct pool *p, const char *str);
char *pool_ptr(struct pool *p, int h);

void
pool_stat(struct pool *p)
{
  fprintf(stderr, "(pool used: %d size: %d dsize: %d buf: %p)\n",
          p->used, p->size, p->dsize, p->buf);
}

int
pool_new(struct pool *p, int size)
{
  int ret = -1;

  p = p ? p : &pool;
  if (p->used + size < p->size) {
    ret = p->used;
    p->used += size;
  } else {
    while (p->used + size >= p->size)
      p->size += p->dsize;
    p->buf = realloc(p->buf, p->size);
    if (!p->buf)
      return -1;
    ret = p->used;
    p->used += size;
  }
  return ret;
}

void
pool_clean(struct pool *p)
{
  p = p ? p : &pool;
  if (p) {
    free(p->buf);
    p->buf = 0;
    p->used = 0;
    p->size = 0;
  }
}

int
pool_new_strn(struct pool *p, const char *str, int len)
{
  int h;
  char *b;
  
  p = p ? p : &pool;
  h = pool_new(p, len + 1);
  b = pool_ptr(p, h);
  if (b) {
    memcpy(b, str, len);
    b[len] = 0;
  }
  return h;
}

int
pool_new_str(struct pool *p, const char *str)
{
  return pool_new_strn(p, str, strlen(str));
}

char *
pool_ptr(struct pool *p, int h)
{
  p = p ? p : &pool;
  if (!p || !p->buf || h == POOL_NIL || h >= p->used)
    return 0;
  return (char *)p->buf + h;
}

int
pool_state(struct pool *p)
{
  return p->used;
}

void
pool_restore(struct pool *p, int mark)
{
  p->used = mark;
}

int
pool_append_str(struct pool *p, int h, const char *str)
{
  int t;

  if (!str)
    return POOL_NIL;

  if (h != POOL_NIL && p->used > 0)
    p->used--;
  t = pool_new_str(p, str);
  return (h == POOL_NIL) ? t : h;
}

int
pool_append_char(struct pool *p, int h, char c)
{
  char buf[2];

  buf[0] = c;
  buf[1] = 0;
  return pool_append_str(p, h, buf);
}

#if 0
int
pool_main()
{
  int s[10], i, n;
  char buf[10];

  s[0] = pool_new_str(0, "one");
  for (i = 1; i < 10; ++i) {
    snprintf(buf, sizeof(buf), "%x", i);
    n = strlen(buf);
    s[i] = pool_new(0, n);
    memcpy(pool_ptr(0, s[i]), buf, n);
  }
  i = pool_new(0, 1);
  pool_ptr(0, i)[0] = 0;
  for (i = 0; i < 10; ++i) {
    fprintf(stderr, "%02d %d '%s'\n", i, s[i], pool_ptr(0, s[i]));
  }
  pool_clean(0);
  return 0;
}
#endif
