#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "pool.h"
#include "node.h"
#include "input.h"

struct input {
  void *context;
  int (*get)(void *context);
  void (*unget)(int c, void *context);
};

struct xml_symbol {
  char *id;
  int c;
} xml_symbols [] = {
  {"gt", '>'},
  {"lt", '<'},
  {"amp", '&'},
  {"apos", '\''},
  {"quot", '"'},
  {0}
};

int xml_read_node(struct input *in, struct pool *p, int *ret);

int
in_getc(struct input *in)
{
  /*int c =  in->get(in->context);*/
  int c = input_getc();
  /*fprintf(stderr, "c: %c\n", c);*/
  return c;
}

void
in_ungetc(int c, struct input *in)
{
  /*in->unget(c, in->context);*/
  input_ungetc(c);
}

int
skip_ws(struct input *in)
{
  int c;

  do {
    c = in_getc(in);
    if (c == EOF)
      return -1;
  } while (isspace(c));

  in_ungetc(c, in);
  return 0;
}

int
xml_skip_comment(struct input *in)
{
  int c;

  char end[4] = "-->", *pend = end;
  c = in_getc(in);
  if (c != '-' || c == EOF)
    return -1;
  while (1) {
    c = in_getc(in);
    if (c == EOF)
      return -1;
    if (c == *pend) {
      pend++;
      if (!*pend)
        break;
    } else
      pend = end;
  }
  return 0;
}

int
xml_read_symbol(struct input *in, struct pool *p, int data, int *ret)
{
  char buf[8], *fmt;
  int c, i, len = 0;

  do {
    c = in_getc(in);
    if (c == EOF)
      return -1;
    buf[len++] = c;
  } while (len < sizeof(buf) - 1 && c != ';');
  buf[len - 1] = 0;
  if (buf[0] == '#') {
    if (buf[1] == 'x') {
      if (sscanf(buf + 2, "%x", &i) != 1)
        return 1;
    } else if (sscanf(buf + 1, "%u", &i) != 1)
      return 1;
    data = pool_append_char(p, data, i);
  } else {
    for (i = 0; xml_symbols[i].id; i++)
      if (!strcmp(xml_symbols[i].id, buf)) {
        data = pool_append_char(p, data, xml_symbols[i].c);
        break;
      }
    if (!xml_symbols[i].id)
      return -1;
  }
  *ret = data;
  return 0;
}

void
show_pool_str(int s, struct pool *p)
{
}

int
xml_read_cdata(struct input *in, struct pool *p, int data, int *ret)
{
  int c, mark, t;
  char end[4] = "]]>", *pend = end, *pc;

  mark = pool_state(p);

  c = in_getc(in);
  if (c != '[')
    goto error;
  while(1) {
    c = in_getc(in);
    if (c == EOF)
      goto error;
    if (c == *pend) {
      pend++;
      if (!*pend)
        break;
    } else {
      for (pc = end; pc != pend; pc++)
        data = pool_append_char(p, data, *pc);
      pend = end;
      data = pool_append_char(p, data, c);
    }
  }
  *ret = data;
  return 0;
error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_id(struct input *in, struct pool *p, int *ret)
{
  int c, mark, id;

  mark = pool_state(p);
  if (skip_ws(in))
    goto error;
  
  c = in_getc(in);
  if (!isalpha(c))
    goto error;
  id = pool_append_char(p, POOL_NIL, c);
  while (1) {
    c = in_getc(in);
    if (isalnum(c) || c == '_' || c == '-' || c == ':' || c == '.')
      pool_append_char(p, id, c);
    else
      break;
  }
  in_ungetc(c, in);
  *ret = id;
  return 0;

error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_attr_val(struct input *in, struct pool *p, int *quote, int *ret)
{
  int c, mark, q, val;

  mark = pool_state(p);
  if (skip_ws(in))
    goto error;

  q = in_getc(in);
  if (q != '\'' && q != '"')
    goto error;

  val = -1;
  c = in_getc(in);
  if (c == q) {
    *ret = -1;
    return 0;
  }

  val = pool_append_char(p, POOL_NIL, c);
  if (val < 0)
    goto error;
  while (1) {
    c = in_getc(in);
    if (c == EOF)
      goto error;
    if (c == q)
      break;
    if (pool_append_char(p, val, c) < 0)
      goto error;
  }
  *quote = q;
  *ret = val;
  return 0;

error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_attr(struct input *in, struct pool *p, int attrlist, int *ret)
{
  int c, mark, id, val, a, q;
  struct xml_attr *attr;

  mark = pool_state(p);

  if (xml_read_id(in, p, &id))
    goto error;

  if (skip_ws(in))
    goto error;

  c = in_getc(in);

  if (c != '=') 
    goto error;

  if (xml_read_attr_val(in, p, &q, &val))
    goto error;

  *ret = xml_make_attr(id, val, q, attrlist, p);
  if (*ret == POOL_NIL)
    goto error;
  return 0;

error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_attrs(struct input *in, struct pool *p, int *ret)
{
  int c, mark, attrs = -1;

  mark = pool_state(p);

  while (1) {
    if (skip_ws(in))
      goto error;
    c = in_getc(in);
    in_ungetc(c, in);
    if (!isalpha(c)) {
      *ret = attrs;
      return 0;
    }
    if (xml_read_attr(in, p, attrs, &attrs))
      goto error;
  }
error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_node_head(struct input *in, struct pool *p, int *ret)
{
  int mark, id, attrs = -1;

  *ret = -1;
  mark = pool_state(p);

  if (xml_read_id(in, p, &id))
    goto error;
  if (xml_read_attrs(in, p, &attrs))
    goto error;
  *ret = xml_make_node(id, attrs, -1, p);
  return 0;

error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_node_end(struct input *in, struct pool *p, int node)
{
  int c, mark, id;
  char *name, *end;

  mark = pool_state(p);

  if (xml_read_id(in, p, &id))
    goto error;
  if (skip_ws(in))
    goto error;
  c = in_getc(in);
  if (c != '>')
    goto error;

  name = xml_node_name(node, p);
  end = pool_ptr(p, id);
  if (!name || !end || strcmp(name, end) != 0)
    goto error;

  pool_restore(p, mark);
  return 0;

error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_tagged(struct input *in, struct pool *p, int *data, int *datalist)
{
  int c, mark, tmp, n;

  mark = pool_state(p);

  c = in_getc(in);
  if (c == '!') {
    c = in_getc(in);
    switch (c) {
    case '-':
      if (xml_skip_comment(in))
        goto error;
      break;
    case '[':
      tmp = pool_state(p);
      if (xml_read_id(in, p, &n) || !pool_ptr(p, n))
        goto error;
      if (!strcmp(pool_ptr(p, n), "CDATA")) {
        pool_restore(p, tmp);
        if (xml_read_cdata(in, p, *data, data))
          goto error;
      } else
        goto error;
      break;
    default:
      goto error;
    }
  } else {
    in_ungetc(c, in);
    if (xml_read_node(in, p, &n))
      goto error;
    *datalist = xml_add_data(n, XML_NODE, *datalist, p);
    if (*datalist < 0)
      goto error;
  }
  return 0;

error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_node_data(struct input *in, struct pool *p, int node, int datalist, 
                   int *ret)
{
  int c, mark, t, data = -1;

  mark = pool_state(p);
  while (1) {
    c = in_getc(in);
    if (c == EOF)
      goto error;
    if (c == '<') {
      if (data >= 0) {
        datalist = xml_add_data(data, XML_TEXT, datalist, p);
        if (datalist < 0)
          goto error;
        data = -1;
      }
      c = in_getc(in);
      if (c == '/') {
        if (xml_read_node_end(in, p, node))
          goto error;
        break;
      } else {
        in_ungetc(c, in);
        if (xml_read_tagged(in, p, &data, &datalist))
          goto error;
      }
    } else if (c == '&') {
      if (xml_read_symbol(in, p, data, &data))
        goto error;
    } else
      data = pool_append_char(p, data, c);
  }
  *ret = datalist;
  return 0;

error:
  pool_restore(p, mark);
  return -1;
}

int
xml_read_node(struct input *in, struct pool *p, int *ret)
{
  int c, mark, n, data = -1;
  mark = pool_state(p);

  if (xml_read_node_head(in, p, &n))
    goto error;

  if (skip_ws(in))
    goto error;

  c = in_getc(in);
  switch (c) {
  case '>':
    if (xml_read_node_data(in, p, n, -1, &data))
      goto error;
    if (xml_set_node_data(n, data, p))
      goto error;
    break;
  case '/':
    c = in_getc(in);
    if (c != '>')
      goto error;
    break;
  default:
    goto error;
  }
  *ret = n;
  return 0;

error:
  pool_restore(p, mark);
  return -1;
}
