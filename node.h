enum xml_type {
  XML_NODE,
  XML_TEXT
};

struct xml_attr {
  int name;
  int value;
  int quote;
  int next;
};

struct xml_data {
  enum xml_type type;
  int next;
  int value;
};

struct xml_node {
  int name;
  int attr;
  int data;
};

int xml_make_node(int name, int attrs, int data, struct pool *p);
int xml_make_attr(int name, int value, int next, struct pool *p);
int xml_add_attr(int attr, int attrlist, struct pool *p);
int xml_add_data(int x, enum xml_type type, int list, struct pool *p);

char *xml_node_name(int node, struct pool *p);
char *xml_node_text(int node, struct pool *p);
char *xml_attr_value(int attr, struct pool *p);

int xml_new(const char *name, struct pool *p);
int xml_node_add_attr(int node, char *id, char *value, struct pool *p);
int xml_node_add_text(int node, const char *text, struct pool *p);
int xml_node_add_text_id(int node, int text, struct pool *p);

struct xml_data *xml_node_data(int node, struct pool *p);
struct xml_data *xml_data_next(struct xml_data *d, struct pool *p);
int xml_node_find_attr(int node, char *name, struct pool *p);
int xml_node_find(int node, char *name, struct pool *p);

int reverse_datalist(int datalist, struct pool *p);
int xml_set_node_data(int node, int datalist, struct pool *p);
char *str_from_xml_node(struct pool *pool, int node, struct pool *nodepool);

int xml_append_esc(struct pool *pool, int h, const char *str);
int xml_printf(struct pool *pool, int h, const char *fmt, ...);
