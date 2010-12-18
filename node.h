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
int xml_make_attr(int name, int value, int quote, int next, struct pool *p);
int xml_add_attr(int attr, int attrlist, struct pool *p);
int xml_add_data(int x, enum xml_type type, int list, struct pool *p);

int reverse_datalist(int datalist, struct pool *p);
char *xml_node_name(int node, struct pool *p);
int xml_set_node_data(int node, int datalist, struct pool *p);
char *str_from_xml_node(struct pool *pool, int node, struct pool *nodepool);
