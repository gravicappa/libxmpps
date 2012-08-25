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
#include <string.h>
#include "pool.h"

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

  if (p->used + size < p->size) {
    ret = p->used;
    p->used += size;
  } else {
    if (p->dsize)
      while (p->used + size >= p->size)
        p->size += p->dsize;
    else
      p->size = p->used + size;
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
pool_append_strn(struct pool *p, int h, const char *str, int len)
{
  int t;

  if (!str)
    return POOL_NIL;

  if (h != POOL_NIL && p->used > 0)
    p->used--;
  t = pool_new_strn(p, str, len);
  return (h == POOL_NIL) ? t : h;
}


int
pool_append_str(struct pool *p, int h, const char *str)
{
  return pool_append_strn(p, h, str, str ? strlen(str) : 0);
}

int
pool_append_char(struct pool *p, int h, char c)
{
  char buf[2];

  buf[0] = c;
  buf[1] = 0;
  return pool_append_str(p, h, buf);
}
