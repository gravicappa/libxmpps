#include <stdio.h>
#include "pool.h"
#include "node.h"
#include "input.h"
#include "xml.h"

struct string_buf {
  char *buf;
  int off;
};

int
getbuf(void *user, int len, char *buf)
{
  struct string_buf *sb = (struct string_buf *)user;
  if (sb->buf == 0 || sb->buf[sb->off] == 0)
    return 0;
  buf[0] = sb->buf[sb->off++];
  return 1;
}

void
test_id()
{
  struct string_buf sb = {"a_a:34 = 'alpha'   b=\"beta'gamma\""};
  struct pool p = {1024};
  int ret, a;

  set_input_callback(&sb, getbuf);
  ret = xml_read_id(0, &p, &a);
  fprintf(stderr, "ret: %d id: '%s'\n", ret, pool_ptr(&p, a));
}

void
test_attr()
{
  struct string_buf sb = { "a = 'alpha beta \"gamma\"' b=\"beta'gamma\"" };
  struct pool p = { 1024 };
  int ret, a;
  struct xml_attr *attr;

  set_input_callback(&sb, getbuf);
  ret = xml_read_attr(0, &p, -1, &a);
  attr = (struct xml_attr *)pool_ptr(&p, a);
  if (attr) {
    fprintf(stderr, "id: '%s' val: '%s'\n", pool_ptr(&p, attr->name),
            pool_ptr(&p, attr->value));
  }
}

void
test_node_start()
{
  struct string_buf sb = {"one a=\"gamma\" b=\"delta'epsilon\">"};
  struct pool p = {1024};
  int ret, n;

  set_input_callback(&sb, getbuf);
  ret = xml_read_node_head(0, &p, &n);
  fprintf(stderr, "node: %d %p\n", n, pool_ptr(&p, n));
  fprintf(stderr, "xml:\n%s\n", str_from_xml_node(&p, n, &p));
}

void
test_node_cdata()
{
  struct string_buf sb = {"[lalala<bububu>b]>e]sd]]sd]]]]f]]>"};
  struct pool p = {1024};
  int ret, n;

  set_input_callback(&sb, getbuf);
  ret = xml_read_cdata(0, &p, POOL_NIL, &n);
  fprintf(stderr, "cdata: '%s'\n", pool_ptr(&p, n));
}

void
test_node()
{
  struct string_buf sb = {"one a='gamma' b=\"delta'epsilon\">"
                          "afasdf\nlalal"
                          "<!-- lalala <one> -->"
                          "&lt;three&gt; ban&#x32;i"
                          "<two>data</two>"
                          "<![CDATA[{<one>bububu</one>}]]>"
                          "</one>"};
  struct pool p = {1024};
  int ret, n, x, a;

  set_input_callback(&sb, getbuf);
  ret = xml_read_node(0, &p, &n);
  fprintf(stderr, "ret: %d node: %d %p\n", ret, n, pool_ptr(&p, n));
  fprintf(stderr, "xml:\n%s\n", str_from_xml_node(&p, n, &p));
  x = xml_node_find(n, "two", &p);
  fprintf(stderr, "two text: '%s'\n", xml_node_text(x, &p));
  fprintf(stderr, "two:\n%s\n", str_from_xml_node(&p, x, &p));
  a = xml_node_find_attr(n, "a", &p);
  fprintf(stderr, "one/a: '%s'\n", xml_attr_value(a, &p));
  pool_stat(&p);
}

int
main()
{
  /*
  test_id();
  test_attr();
  test_node_start();
  */
  test_node_cdata();
  test_node();
  return 0;
}
