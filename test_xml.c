#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pool.h"
#include "node.h"
#include "xml.h"
#include "xml_states.h"

static int dbg_show_fsm = 0;

void
test_xml()
{
  struct xml xml = { {4096}, {4096} };
  struct pool mem = { 4096 };
  int c;
  char *id;

  if (xml_init(&xml))
    exit(1);

  while ((c = getc(stdin)) != EOF) {
    if (dbg_show_fsm)
      fprintf(stderr, "s: %d c: '%c' ", xml.state, c);
    if (xml_next_char(c, &xml))
      break;
    switch (xml.state) {
    case XML_STATE_NODE_HEAD:
      fprintf(stderr, "XML_STATE_NODE_HEAD level: %d\n", xml.level);
      if (xml.level == 1) {
        id = pool_ptr(&xml.mem, xml.node_id);
        fprintf(stderr, "started '%s'\n", id);
        if (id && (!strcmp(id, "stream") || !strcmp(id, "stream:stream"))) {
          fprintf(stderr, "\nSTREAM started\n%s\n\n",
                  str_from_xml_node(&mem, xml.node, &xml.mem));
        }
      }
      break;
    case XML_STATE_NODE_END:
      fprintf(stderr, "XML_STATE_NODE_END level: %d\n", xml.level);
      if (xml.level == 1) {
        fprintf(stderr, "\nNODE\n%s\n\n",
                str_from_xml_node(&mem, xml.last_node, &xml.mem));
        id = xml_node_text(xml_node_find(xml.last_node, "body", &xml.mem),
                           &xml.mem);
        fprintf(stderr, "\nMSG\n%s\n\n", id);
      }
      break;
    }
    if (dbg_show_fsm)
      fprintf(stderr, "=> %d\n", xml.state);
    if (xml.state < 0)
      break;
  }
  xml_clean(&xml);
  pool_clean(&mem);
}

void
test_printf()
{
  struct pool mem = { 4096 };
  char *s;

  s = xml_sprintf(&mem, POOL_NIL, "<%S attr='%s' id='%s_%d'>%s</%S>", "one",
                  "two\"three'four", "session", 167, "five<six>seven'eight'",
                  "one");
  fprintf(stderr, "h: '%s'\n", s);
  pool_clean(&mem);
}

void
test_make_xml()
{
  struct pool mem = { 4096 };
  int x;

  x = xml_new("some_tag", &mem);
  xml_node_add_text(x, "Data data data.", &mem);
  xml_node_add_attr(x, "attr", "567", &mem);
  xml_node_add_text(x, " More data.", &mem);
  xml_node_add_attr(x, "alpha", "α='1.3'", &mem);
  xml_node_add_attr(x, "beta", "β='2.3'", &mem);
  xml_insert(x, "inside", &mem);

  fprintf(stderr, "\nNODE\n%s\n\n", str_from_xml_node(&mem, x, &mem));
  pool_clean(&mem);
}

int
main()
{
  if (1)
    test_xml();
  if (1)
    test_printf();
  if (1)
    test_make_xml();
  return 0;
}

