#include <stdio.h>
#include "pool.h"
#include "node.h"
#include "xml.h"
#include "xml_states.h"

void
test_xml()
{
  struct xml xml = { {4096}, {4096} };
  struct pool mem = { 4096 };
  int n, c;
  char *id;

  if (xml_init(&xml))
    exit(1);

  while ((c = getc(stdin)) != EOF) {
    fprintf(stderr, "s: %d c: '%c' ", xml.state, c);
    if (xml_next_char(c, &xml))
      break;
    if (xml.state == XML_STATE_CONTENTS) {
      if (xml.level == 0) {
        fprintf(stderr, "\nNODE\n%s\n\n",
                str_from_xml_node(&mem, xml.last_node, &xml.mem));
      } else if (xml.level == 1) {
        id = pool_ptr(&xml.mem, xml.node_id);
        if (id && !strcmp(id, "stream")) {
          fprintf(stderr, "\nSTREAM started\n");
        }
      }
    }
    fprintf(stderr, "=> %d\n", xml.state);
    if (xml.state < 0)
      break;
  }
  xml_clean(&xml);
}

int
main()
{
  test_xml();
  return 0;
}

