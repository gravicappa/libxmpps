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
#include <string.h>
#include <stdarg.h>
#include "pool.h"
#include "node.h"

int
xml_append_esc(struct pool *pool, int h, const char *str)
{
  int mark;

  if (!str)
    return h;
  mark = pool_state(pool);
  for (; *str; str++) {
    switch (*str) {
    case '\'': h = pool_append_str(pool, h, "&apos;"); break;
    case '"': h = pool_append_str(pool, h, "&quot;"); break;
    case '&': h = pool_append_str(pool, h, "&amp;"); break;
    case '<': h = pool_append_str(pool, h, "&lt;"); break;
    case '>': h = pool_append_str(pool, h, "&gt;"); break;
    default: h = pool_append_char(pool, h, *str);
    }
    if (h == POOL_NIL) {
      pool_restore(pool, mark);
      break;
    }
  }
  return h;
}

int
xml_vprintf(struct pool *pool, int h, const char *fmt, va_list args)
{
  int mark;
  char buf[32];
  const char *prev, *s, *arg;

  mark = pool_state(pool);
  prev = fmt;
  while ((s = strchr(prev, '%'))) {
    h = pool_append_strn(pool, h, prev, s - prev);
    s++;
    switch (*s) {
    case 'c':
      buf[0] = (char)va_arg(args, int);
      buf[1] = 0;
      h = xml_append_esc(pool, h, buf);
      break;
    case 'C':
      h = pool_append_char(pool, h, (char)va_arg(args, int));
      break;
    case 's':
      arg = va_arg(args, const char *);
      h = (arg) ? xml_append_esc(pool, h, arg) : POOL_NIL;
      break;
    case 'S':
      arg = va_arg(args, const char *);
      h = (arg) ? pool_append_str(pool, h, arg) : POOL_NIL;
      break;
    case 'd':
      snprintf(buf, sizeof(buf), "%d", va_arg(args, int));
      h = pool_append_str(pool, h, buf);
      break;
    case 'u':
      snprintf(buf, sizeof(buf), "%u", va_arg(args, unsigned int));
      h = pool_append_str(pool, h, buf);
      break;
    case '%':
      h = pool_append_char(pool, h, '%');
      break;
    default:
      h = POOL_NIL;
    }
    prev = s + 1;
    if (h == POOL_NIL)
      break;
  }
  h = pool_append_str(pool, h, prev);
  if (h == POOL_NIL)
    pool_restore(pool, mark);
  return h;
}

int
xml_printf(struct pool *pool, int h, const char *fmt, ...)
{
  int ret;
  va_list args;
  va_start(args, fmt);
  ret = xml_vprintf(pool, h, fmt, args);
  va_end(args);
  return ret;
}

char *
xml_sprintf(struct pool *pool, int h, const char *fmt, ...)
{
  char *ret;
  va_list args;
  va_start(args, fmt);
  ret = pool_ptr(pool, xml_vprintf(pool, h, fmt, args));
  va_end(args);
  return ret;
}

static int
reverse_attrlist(int attrlist, struct pool *pool)
{
  struct xml_attr *a;
  int i, j, t;

  t = -1;
  for (i = attrlist; i != POOL_NIL;) {
    a = (struct xml_attr *)pool_ptr(pool, i);
    j = a->next;
    a->next = t;
    t = i;
    i = j;
  }
  return t;
}

int
reverse_datalist(int datalist, struct pool *p)
{
  int i, j, t;
  struct xml_data *d;

  t = -1;
  for (i = datalist; i != POOL_NIL;) {
    d = (struct xml_data *)pool_ptr(p, i);
    j = d->next;
    d->next = t;
    t = i;
    i = j;
  }
  return t;
}

int
xml_make_node(int name, int attrs, int data, struct pool *p)
{
  int h;
  struct xml_node *n;

  if (name == POOL_NIL)
    return POOL_NIL;

  h = pool_new(p, sizeof(struct xml_node));
  if (h != POOL_NIL) {
    n = (struct xml_node *)pool_ptr(p, h);
    n->name = name;
    n->attr = reverse_attrlist(attrs, p);
    n->data = reverse_datalist(data, p);
  }
  return h;
}

int
xml_make_attr(int name, int value, int next, struct pool *p)
{
  int h;
  struct xml_attr *a;

  if (name == POOL_NIL)
    return POOL_NIL;
  h = pool_new(p, sizeof(struct xml_attr));
  if (h != POOL_NIL) {
    a = (struct xml_attr *)pool_ptr(p, h);
    a->name = name;
    a->value = value;
    a->next = next;
  }
  return h;
}

static int
xml_make_attr_s(const char *name, const char *value, int next, struct pool *p)
{
  int mark, h, s;

  mark = pool_state(p);
  s = pool_new_str(p, value);
  h = xml_make_attr(pool_new_str(p, name), s, next, p);
  if (h == POOL_NIL || !pool_ptr(p, s))
    pool_restore(p, mark);
  return h;
}

struct xml_data *
xml_node_data(int node, struct pool *p)
{
  struct xml_node *n;
  n = (struct xml_node *)pool_ptr(p, node);
  return n ? (struct xml_data *)pool_ptr(p, n->data) : 0;
}

struct xml_data *
xml_data_next(struct xml_data *d, struct pool *p)
{
  return d ? (struct xml_data *)pool_ptr(p, d->next) : 0;
}

int
xml_node_add_data(int node, int data, struct pool *p)
{
  struct xml_node *n = 0;
  struct xml_data *d, *last = 0;

  if (data == POOL_NIL)
    return -1;

  for (d = xml_node_data(node, p); d; d = xml_data_next(d, p))
    last = d;

  if (last)
    last->next = data;
  else {
    n = (struct xml_node *)pool_ptr(p, node);
    if (!n)
      return -1;
    n->data = data;
  }
  return 0;
}

int
xml_node_add_text_id(int node, int text, struct pool *p)
{
  int h, mark;
  mark = pool_state(p);
  h = xml_add_data(text, XML_TEXT, POOL_NIL, p);
  if (h == POOL_NIL || xml_node_add_data(node, h, p)) {
    pool_restore(p, mark);
    return 1;
  }
  return 0;
}

int
xml_node_add_text(int node, const char *text, struct pool *p)
{
  return xml_node_add_text_id(node, pool_new_str(p, text), p);
}

int
xml_node_add_textn(int node, int len, const char *text, struct pool *p)
{
  return xml_node_add_text_id(node, pool_new_strn(p, text, len), p);
}

int
xml_add_data(int x, enum xml_type type, int list, struct pool *p)
{
  int h;
  struct xml_data *d;

  h = pool_new(p, sizeof(struct xml_data));
  if (h != POOL_NIL) {
    d = (struct xml_data *)pool_ptr(p, h);
    d->type = type;
    d->value = x;
    d->next = list;
  }
  return h;
}

int
h_from_xml_node(int h, struct pool *pool, int node, struct pool *nodepool)
{
  struct xml_node *n;
  struct xml_attr *a;
  struct xml_data *d;
  int mark;

  mark = pool_state(pool);
  n = (struct xml_node *)pool_ptr(nodepool, node);
  if (!n)
    return POOL_NIL;

  h = xml_printf(pool, h, "<%S", pool_ptr(nodepool, n->name));
  for (a = (struct xml_attr *)pool_ptr(nodepool, n->attr);
       a;
       a = (struct xml_attr *)pool_ptr(nodepool, a->next))
    h = xml_printf(pool, h, " %S='%s'", pool_ptr(nodepool, a->name),
                   pool_ptr(nodepool, a->value));
  d = (struct xml_data *)pool_ptr(nodepool, n->data);
  if (d) {
    h = pool_append_str(pool, h, ">");
    for (; d; d = (struct xml_data *)pool_ptr(nodepool, d->next))
      switch (d->type) {
      case XML_NODE:
        h = h_from_xml_node(h, pool, d->value, nodepool);
        break;
      case XML_TEXT:
        h = xml_append_esc(pool, h, pool_ptr(nodepool, d->value));
        break;
      }
    h = xml_printf(pool, h, "</%S>", pool_ptr(nodepool, n->name));
  } else
    h = pool_append_str(pool, h, "/>");

  if (h == POOL_NIL)
    pool_restore(pool, mark);
  return h;
}

char *
str_from_xml_node(struct pool *pool, int node, struct pool *nodepool)
{
  return pool_ptr(pool, h_from_xml_node(POOL_NIL, pool, node, nodepool));
}

int
xml_new(const char *name, struct pool *p)
{
  return xml_make_node(pool_new_str(p, name), POOL_NIL, POOL_NIL, p);
}

int
xml_insert(int x, const char *name, struct pool *p)
{
  int n, h, mark;

  if (x == POOL_NIL)
    return POOL_NIL;

  mark = pool_state(p);
  n = xml_new(name, p);
  if (n == POOL_NIL) {
    pool_restore(p, mark);
    return POOL_NIL;
  }
  h = xml_add_data(n, XML_NODE, POOL_NIL, p);
  if (h == POOL_NIL || xml_node_add_data(x, h, p)) {
    pool_restore(p, mark);
    return POOL_NIL;
  }
  return n;
}

int
xml_node_add_attr(int node, const char *id, const char *value, struct pool *p)
{
  struct xml_node *n;
  int a;
  n = (struct xml_node *)pool_ptr(p, node);
  if (!n)
    return -1;
  a = xml_make_attr_s(id, value, n->attr, p);
  if (a == POOL_NIL)
    return -1;
  n->attr = a;
  return 0;
}

int
xml_node_find(int node, const char *name, struct pool *p)
{
  struct xml_data *d;
  char *s;

  for (d = xml_node_data(node, p); d; d = xml_data_next(d, p))
    if (d->type == XML_NODE) {
      s = xml_node_name(d->value, p);
      if (s && !strcmp(name, s))
        return d->value;
    }
  return POOL_NIL;
}

char *
xml_node_text(int node, struct pool *p)
{
  struct xml_data *d;
  for (d = xml_node_data(node, p); d; d = xml_data_next(d, p))
    if (d->type == XML_TEXT)
      return pool_ptr(p, d->value);
  return 0;
}

char *
xml_node_find_text(int node, const char *name, struct pool *p)
{
  return xml_node_text(xml_node_find(node, name, p), p);
}

char *
xml_node_name(int node, struct pool *p)
{
  struct xml_node *n;
  n = (struct xml_node *)pool_ptr(p, node);
  return n ? pool_ptr(p, n->name) : 0;
}

char *
xml_node_find_attr(int node, const char *name, struct pool *p)
{
  struct xml_node *n;
  struct xml_attr *a;
  int attr;
  char *s;

  n = (struct xml_node *)pool_ptr(p, node);
  if (!n)
    return 0;
  attr = n->attr;
  while (attr != POOL_NIL) {
    a = (struct xml_attr *)pool_ptr(p, attr);
    if (!a)
      return 0;
    s = pool_ptr(p, a->name);
    if (s && !strcmp(s, name))
      return pool_ptr(p, a->value);
    attr = a->next;
  }
  return 0;
}
