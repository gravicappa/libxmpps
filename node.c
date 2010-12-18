#include <stdio.h>
#include "pool.h"
#include "node.h"

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
xml_make_attr(int name, int value, int quote, int next, struct pool *p)
{
  int h;
  struct xml_attr *a;

  h = pool_new(p, sizeof(struct xml_attr));
  if (h != POOL_NIL) {
    a = (struct xml_attr *)pool_ptr(p, h);
    a->name = name;
    a->value = value;
    a->quote = quote;
    a->next = next;
  }
  return h;
}

int
xml_set_node_data(int node, int datalist, struct pool *p)
{
  struct xml_node *n;

  n = (struct xml_node *)pool_ptr(p, node);
  if (!n)
    return 1;
  n->data = reverse_datalist(datalist, p);
  return 0;
}

int
xml_add_attr(int attr, int attrlist, struct pool *p)
{
  struct xml_attr *a;

  a = (struct xml_attr *)pool_ptr(p, attr);
  if (!a)
    return -1;
  a->next = attrlist;
  return attr;
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

char *
xml_node_name(int node, struct pool *p)
{
  struct xml_node *n;
  n = (struct xml_node *)pool_ptr(p, node);
  return n ? pool_ptr(p, n->name) : 0;
}

char *
xml_attr_value(int attr, struct pool *p)
{
  struct xml_attr *a;
  a = (struct xml_attr *)pool_ptr(p, attr);
  return a ? pool_ptr(p, a->value) : 0;
}

int
xml_node_find_attr(int node, char *name, struct pool *p)
{
  struct xml_node *n;
  struct xml_attr *a;
  int attr;
  char *s;

  n = (struct xml_node *)pool_ptr(p, node);
  if (!n)
    return POOL_NIL;
  attr = n->attr;
  while (attr != POOL_NIL) {
    a = (struct xml_attr *)pool_ptr(p, attr);
    if (!a)
      return POOL_NIL;
    s = pool_ptr(p, a->name);
    if (s && !strcmp(s, name))
      return attr;
    attr = a->next;
  }
  return POOL_NIL;
}

int
xml_node_find(int node, char *name, struct pool *p)
{
  struct xml_node *n;
  struct xml_data *d;
  int data;
  char *s;

  n = (struct xml_node *)pool_ptr(p, node);
  if (!n)
    return POOL_NIL;
  data = n->data;
  while (data != POOL_NIL) {
    d = (struct xml_data *)pool_ptr(p, data);
    if (!d)
      return POOL_NIL;
    if (d->type == XML_NODE) {
      s = xml_node_name(d->value, p);
      if (s && !strcmp(name, s))
        return d->value;
    }
    data = d->next;
  }
  return POOL_NIL;
}

char *
xml_node_text(int node, struct pool *p)
{
  struct xml_node *n;
  struct xml_data *d;
  int data;
  char *s;

  n = (struct xml_node *)pool_ptr(p, node);
  if (!n)
    return POOL_NIL;
  data = n->data;
  while (data != POOL_NIL) {
    d = (struct xml_data *)pool_ptr(p, data);
    if (!d)
      return POOL_NIL;
    if (d->type == XML_TEXT)
      return pool_ptr(p, d->value);
    data = d->next;
  }
  return POOL_NIL;
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

  h = pool_append_str(pool, h, "<");
  h = pool_append_str(pool, h, pool_ptr(nodepool, n->name));
  for (a = (struct xml_attr *)pool_ptr(nodepool, n->attr);
       a;
       a = (struct xml_attr *)pool_ptr(nodepool, a->next)) {
      h = pool_append_str(pool, h, " ");
      h = pool_append_str(pool, h, pool_ptr(nodepool, a->name));
      h = pool_append_str(pool, h, "=");
      h = pool_append_char(pool, h, a->quote);
      h = pool_append_str(pool, h, pool_ptr(nodepool, a->value));
      h = pool_append_char(pool, h, a->quote);
    }
  d = (struct xml_data *)pool_ptr(nodepool, n->data);
  if (d) {
    h = pool_append_str(pool, h, ">");
    for (; d; d = (struct xml_data *)pool_ptr(nodepool, d->next)) {
      switch (d->type) {
      case XML_NODE:
        h = h_from_xml_node(h, pool, d->value, nodepool);
        break;
      case XML_TEXT:
        h = pool_append_str(pool, h, pool_ptr(nodepool, d->value));
        break;
      }
    }
    h = pool_append_str(pool, h, "</");
    h = pool_append_str(pool, h, pool_ptr(nodepool, n->name));
    h = pool_append_str(pool, h, ">");
  } else
    h = pool_append_str(pool, h, "/>");

  return h;
error:
  pool_restore(pool, mark);
  return POOL_NIL;
}

char *
str_from_xml_node(struct pool *pool, int node, struct pool *nodepool)
{
  return pool_ptr(pool, h_from_xml_node(POOL_NIL, pool, node, nodepool));
}
