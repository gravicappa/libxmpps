#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pool.h"
#include "node.h"
#include "fsm.h"
#include "xml.h"
#include "xml_states.h"

static int push_node(int n, int data, struct pool *p, struct pool *stack);
static int pop_node(int *n, int *data, struct pool *p, struct pool *stack);

static int insert_utf8_char(unsigned int x, struct xml *xml);

static int ischarid(int c);
static int ischarhex(int c);

static int id_start(int c, void *context);
static int id_body(int c, void *context);

static int node_id_end(int c, void *context);

static int node_attr_value_end(int c, void *context);

static int node_empty(int c, void *context);
static int node_contents(int c, void *context);

static int node_tail_end(int c, void *context);

static int contents_start(int c, void *context);

static int entity_start(int c, void *context);
static int entity_body(int c, void *context);
static int entity_end(int c, void *context);

static int hex_number_end(int c, void *context);
static int dec_number_start(int c, void *context);
static int dec_number_end(int c, void *context);

static int text_start(int c, void *context);
static int text_body(int c, void *context);
static int node_text_end(int c, void *context);

#define RULE_WHITESPACE(s, next) \
  {s, isspace, 0, s}, \
  {s, fsm_true, 0, next, fsm_reject}

#define RULE_ID(s, next, end) \
  RULE_WHITESPACE(s, s ## _X1), \
  {s ## _X1, isalpha, 0, s ## _X2, id_start}, \
  {s ## _X1, fsm_true, 0, -1}, \
  {s ## _X2, ischarid, 0, s ## _X2, id_body}, \
  {s ## _X2, fsm_true, 0, next, end}

#define RULE_TEXT(s, next) \
  {s, fsm_char, '&', s ## _X1}, \
  {s, fsm_true, 0, s, text_body}, \
  {s ## _X1, fsm_char, '#', s ## _X2}, \
  {s ## _X1, isalpha, 0, s ## _X5, entity_start}, \
  {s ## _X2, fsm_char, 'x', s ## _X3}, \
  {s ## _X2, isdigit, 0, s ## _X4, dec_number_start}, \
  {s ## _X3, ischarhex, 0, s ## _X3, entity_body}, \
  {s ## _X3, fsm_char, ';', s, hex_number_end}, \
  {s ## _X4, isdigit, 0, s ## _X4, entity_body}, \
  {s ## _X4, fsm_char, ';', s, dec_number_end}, \
  {s ## _X5, isalpha, 0, s ## _X5, entity_body}, \
  {s ## _X5, fsm_char, ';', s, entity_end}

#define RULE_ATTR_VALUE(s, next, quote, start, end) \
  {s, fsm_true, 0, s ## _X1, start}, \
  {s ## _X1, fsm_char, quote, next, end}, \
  RULE_TEXT(s ## _X1, next)

#define RULE_NODE_ATTR(s, next) \
  RULE_WHITESPACE(s, s ## _X1), \
  {s ## _X1, fsm_char, '/', next, fsm_reject}, \
  {s ## _X1, fsm_char, '>', next, fsm_reject}, \
  RULE_ID(s ## _X1, s ## _X4, fsm_reject),\
  RULE_WHITESPACE(s ## _X4, s ## _X5), \
  {s ## _X5, fsm_char, '=', s ## _X6}, \
  RULE_WHITESPACE(s ## _X6, s ## _X7), \
  {s ## _X7, fsm_char, '\'', s ## _X8}, \
  {s ## _X7, fsm_char, '"', s ## _X9}, \
  RULE_ATTR_VALUE(s ## _X8, s, '\'', text_start, node_attr_value_end), \
  RULE_ATTR_VALUE(s ## _X9, s, '"', text_start, node_attr_value_end)

struct fsm_rule xml_rules[] = {
  RULE_WHITESPACE(XML_STATE_0, XML_STATE_1),
  {XML_STATE_1, fsm_char, '<', XML_STATE_TAG},
  {XML_STATE_TAG, fsm_char, '?', XML_STATE_HEAD},
  /*{XML_STATE_TAG, fsm_char, '!', XML_STATE_COMMENT}, */
  {XML_STATE_TAG, isspace, 0, XML_STATE_TAG_1},
  {XML_STATE_TAG, isalnum, 0, XML_STATE_NODE, fsm_reject},
  {XML_STATE_TAG_1, isspace, 0, XML_STATE_TAG_1},
  {XML_STATE_TAG_1, isalnum, 0, XML_STATE_NODE, fsm_reject},

  /* xml head */
  /* TODO: parse head appropriately */
  {XML_STATE_HEAD, fsm_char, '?', XML_STATE_HEAD_1},
  {XML_STATE_HEAD, fsm_true, 0, XML_STATE_HEAD},
  {XML_STATE_HEAD_1, fsm_char, '>', XML_STATE_0},

  /* xml comments */
  /* TODO: add comment states */

  /* xml node */
  RULE_WHITESPACE(XML_STATE_NODE, XML_STATE_NODE_1),
  RULE_ID(XML_STATE_NODE_1, XML_STATE_NODE_ATTR, node_id_end),
  RULE_NODE_ATTR(XML_STATE_NODE_ATTR, XML_STATE_NODE_ATTR_END),
  {XML_STATE_NODE_ATTR_END, fsm_char, '/', XML_STATE_NODE_ATTR_END_1},
  {XML_STATE_NODE_ATTR_END, fsm_char, '>', XML_STATE_NODE_HEAD,
   node_contents},
  {XML_STATE_NODE_ATTR_END_1, fsm_char, '>', XML_STATE_NODE_END, node_empty},
  {XML_STATE_NODE_HEAD, fsm_true, 0, XML_STATE_CONTENTS, contents_start},

  /* node contents */
  {XML_STATE_CONTENTS, fsm_char, '<', XML_STATE_CONTENTS_1, node_text_end},
  RULE_TEXT(XML_STATE_CONTENTS, XML_STATE_CONTENTS),
  /*{XML_STATE_CONTENTS_1, fsm_char, '!', XML_STATE_CONTENTS_2},*/
  {XML_STATE_CONTENTS_1, fsm_char, '/', XML_STATE_CONTENTS_3},
  {XML_STATE_CONTENTS_1, fsm_true, 0, XML_STATE_NODE, fsm_reject},
  /*{XML_STATE_CONTENTS_2, fsm_char, '-', XML_STATE_COMMENT},*/
  /*{XML_STATE_CONTENTS_2, fsm_char, '[', XML_STATE_SPECIAL},*/

  /* node end */
  RULE_ID(XML_STATE_CONTENTS_3, XML_STATE_CONTENTS_4, node_tail_end),
  RULE_WHITESPACE(XML_STATE_CONTENTS_4, XML_STATE_CONTENTS_5),
  {XML_STATE_CONTENTS_5, fsm_char, '>', XML_STATE_NODE_END},
  {XML_STATE_NODE_END, fsm_true, 0, XML_STATE_CONTENTS, fsm_reject},

  {0}
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

int
xml_init(struct xml *xml)
{
  xml->node = xml->attrs = xml->data = xml->id = xml->node_id = POOL_NIL;
  xml->text = POOL_NIL;
  xml->level = xml->state = xml->text_ent_len = 0;
  xml->fsm = make_fsm(xml_rules);
  return (xml->fsm == 0) ? -1 : 0;
}

void
xml_reset(struct xml *xml)
{
  xml->state = xml->level = 0;
  pool_clean(&xml->mem);
  pool_clean(&xml->stack);
}

void
xml_clean(struct xml *xml)
{
  if (!xml)
    return;
  if (xml->fsm) {
    free(xml->fsm);
    xml->fsm = 0;
  }
  xml_reset(xml);
}

int
xml_next_char(int c, struct xml *xml)
{
  xml->state = fsm_run(xml->fsm, c, xml->state, xml);
  return xml->state < 0 ? -1 : 0;
}

static int
ischarid(int c)
{
  return isalnum(c) || c == ':' || c == '.' || c == '_' || c == '-';
}

static int
ischarhex(int c)
{
  return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static int
push_node(int n, int data, struct pool *p, struct pool *stack)
{
  struct xml_node *node;
  int h, *ptr;

  node = (struct xml_node *)pool_ptr(p, n);
  if (!node)
    return -1;
  node->data = data;
  h = pool_new(stack, sizeof(n));
  ptr = (int *)pool_ptr(stack, h);
  if (!ptr)
    return -1;
  *ptr = n;
  return 0;
}

static int
pop_node(int *n, int *data, struct pool *p, struct pool *stack)
{
  struct xml_node *node;
  int s, *pn;

  s = stack->used;
  if (s < sizeof(int))
    return 0;

  pn = (int *)pool_ptr(stack, s - sizeof(int));
  if (!pn)
    return POOL_NIL;
  *n = *pn;
  pool_restore(stack, s - sizeof(int));

  node = (struct xml_node *)pool_ptr(p, *n);
  if (!node)
    return -1;
  *data = node->data;
  return 0;
}

static int
id_start(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  xml->id = pool_append_char(&xml->mem, POOL_NIL, c);
  return (xml->id == POOL_NIL) ? -1 : 0;
}

static int
id_body(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  xml->id = pool_append_char(&xml->mem, xml->id, c);
  return (xml->id == POOL_NIL) ? -1 : 0;
}

static int
node_id_end(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  xml->node_id = xml->id;
  xml->id = POOL_NIL;
#if 0
  fprintf(stderr, "node_id_end id: '%s'\n",
          pool_ptr(&xml->mem, xml->node_id));
#endif
  xml->attrs = POOL_NIL;
  return 1;
}

static int
node_attr_value_end(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
#if 0
  fprintf(stderr, "attr '%s' = '%s' prev: %d\n", pool_ptr(&xml->mem, xml->id),
          pool_ptr(&xml->mem, xml->text), xml->attrs);
#endif
  xml->attrs = xml_make_attr(xml->id, xml->text, xml->attrs, &xml->mem);
  xml->id = xml->text = POOL_NIL;
  return (xml->attrs == POOL_NIL) ? -1 : 0;
}

static int
add_node(struct xml *xml)
{
  int n;

#if 0
  fprintf(stderr, "add_node '%s'\n", pool_ptr(&xml->mem, xml->node_id));
#endif
  n = xml_make_node(xml->node_id, xml->attrs, POOL_NIL, &xml->mem);
  if (n == POOL_NIL)
    return POOL_NIL;
  xml->attrs = POOL_NIL;
#if 0
  fprintf(stderr, "add_node_item to %d ", xml->data);
#endif
  xml->data = xml_add_data(n, XML_NODE, xml->data, &xml->mem);
#if 0
  fprintf(stderr, "=> %d\n", xml->data);
#endif
  return (xml->data == POOL_NIL) ? POOL_NIL : n;
}

static int
node_empty(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  int n;

  n = add_node(xml);
  if (n == POOL_NIL)
    return -1;
  xml->last_node = n;
#if 0
  fprintf(stderr, "node_empty '%s'\n", pool_ptr(&xml->mem, xml->node_id));
#endif
  xml->node_id = POOL_NIL;
  return 0;
}

static int
node_contents(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  int n;

#if 0
  fprintf(stderr, "node '%s' contents\n", pool_ptr(&xml->mem, xml->node_id));
#endif

  n = add_node(xml);
  if (n == POOL_NIL)
    return -1;

  if (xml->node != POOL_NIL
      && push_node(xml->node, xml->data, &xml->mem, &xml->stack))
    return -1;
  xml->level++;
  xml->node = n;
  xml->data = POOL_NIL;
  return 0;
}

static int
contents_start(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  xml->node_id = POOL_NIL;
  return 1;
}

static int
node_tail_end(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  struct xml_node *node;
  char *sid, *sname;

  node = (struct xml_node *)pool_ptr(&xml->mem, xml->node);
  if (!node)
    return -1;
  sid = pool_ptr(&xml->mem, xml->id);
  sname = pool_ptr(&xml->mem, node->name);
#if 0
  fprintf(stderr, "node %d:'%s'/'%s' end\n", xml->node, sid, sname);
#endif
  if (!sid || !sname || strcmp(sid, sname) != 0)
    return -1;
  node->data = reverse_datalist(xml->data, &xml->mem);
  xml->last_node = xml->node;
  if (pop_node(&xml->node, &xml->data, &xml->mem, &xml->stack)
      || xml->node == POOL_NIL)
    return -1;
  xml->level--;
  xml->node_id = POOL_NIL;
  return 1;
}

static int
entity_start(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  xml->text_ent_len = 1;
  xml->text_ent[0] = c;
  xml->text_ent[1] = 0;
  return 0;
}

static int
entity_body(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  if (xml->text_ent_len >= sizeof(xml->text_ent) - 1)
    return -1;
  xml->text_ent[xml->text_ent_len++] = c;
  xml->text_ent[xml->text_ent_len] = 0;
  return 0;
}

static int
entity_end(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  int i;

  for (i = 0; xml_symbols[i].id; i++)
    if (!strcmp(xml_symbols[i].id, xml->text_ent))
      break;
  if (!xml_symbols[i].id)
    return -1;
  xml->text = pool_append_char(&xml->mem, xml->text, xml_symbols[i].c);
  if (xml->text == POOL_NIL)
    return -1;
  xml->text_ent[0] = 0;
  xml->text_ent_len = 0;
  return 0;
}

static int
insert_utf8_char(unsigned int x, struct xml *xml)
{
  int off, i;
  unsigned char buf[7] = {0};

  if (x <= 0x7f) {
    buf[0] = x;
    off = 0;
  } else if (x < 0x7ff) {
    buf[0] = 0xc0 | x >> 6;
    off = 6;
  } else if (x < 0xffff) {
    buf[0] = 0xe0 | x >> 12;
    off = 12;
  } else if (x < 0x1fffff) {
    buf[0] = 0xf0 | x >> 18;
    off = 18;
  } else if (x < 0x3ffffff) {
    buf[0] = 0xf8 | x >> 24;
    off = 24;
  } else if (x < 0x7fffffff) {
    buf[0] = 0xfc || x >> 30;
    off = 30;
  }
  i = 1;
  for (; off > 0; off -= 6)
    buf[i++] = ((x >> (off - 6)) & 0x3f) | 0x80;
  xml->text = pool_append_str(&xml->mem, xml->text, (char *)buf);
  return (xml->text != POOL_NIL) ? 0 : -1;
}

static int
hex_number_end(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  unsigned int res;

  res = strtoul(xml->text_ent, 0, 16);
  xml->text_ent[0] = 0;
  xml->text_ent_len = 0;
  return (insert_utf8_char(res, xml) == 0) ? 0 : -1;
}

static int
dec_number_start(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  xml->text_ent_len = 1;
  xml->text_ent[0] = c;
  xml->text_ent[1] = 0;
  return 0;
}

static int
dec_number_end(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  unsigned int res;

  res = atoi(xml->text_ent);
  xml->text_ent[0] = 0;
  xml->text_ent_len = 0;
  return (insert_utf8_char(res, xml) == 0) ? 0 : -1;
}

static int
text_start(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  xml->text = pool_append_char(&xml->mem, POOL_NIL, c);
  return (xml->text == POOL_NIL) ? -1 : 0;
}

static int
text_body(int c, void *context)
{
  struct xml *xml = (struct xml *)context;
  xml->text = pool_append_char(&xml->mem, xml->text, c);
  return (xml->text == POOL_NIL) ? -1 : 0;
}

static int
node_text_end(int c, void *context)
{
  struct xml *xml = (struct xml *)context;

#if 0
  fprintf(stderr, "node_text_end text: '%s'\n",
          pool_ptr(&xml->mem, xml->text));
#endif
  xml->data = xml_add_data(xml->text, XML_TEXT, xml->data, &xml->mem);
  if (xml->data == POOL_NIL)
    return -1;
  xml->text = POOL_NIL;
  return 0;
}
