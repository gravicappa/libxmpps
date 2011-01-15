#define XML_ENTITY_BYTES 64

struct xml {
  struct pool mem;
  struct pool stack;

  struct fsm *fsm;
  int state;
  int level;

  int node;
  int last_node;
  int attrs;
  int data;

  int node_id;
  int id;
  int text;

  char text_ent[XML_ENTITY_BYTES];
  int text_ent_len;
};

int xml_init(struct xml *xml);
void xml_clean(struct xml *xml);
void xml_reset(struct xml *xml);
int xml_next_char(int c, struct xml *xml);
