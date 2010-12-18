#include <stdio.h>
#include "input.h"

static char buffer[1024] = {0};
static char unput_buf[256] = {0};
static int len = 0, pos = 0, unput_pos = 0;
static void *getbuf_private = 0;
static int (*getbuf)(void *user, int len, char *buf) = 0;

void
set_input_callback(void *user, int(*callback)(void *user, int len, char *buf))
{
  getbuf_private = user;
  getbuf = callback;
}

int
input_getc(void)
{
  if (unput_pos > 0)
    return unput_buf[--unput_pos];
  if (pos >= len) {
    if (!getbuf)
      return EOF;
    len = getbuf(getbuf_private, sizeof(buffer), buffer);
    if (!len)
      return EOF;
    pos = 0;
  }
  return buffer[pos++];
}

void
input_ungetc(int c)
{
  if (unput_pos < sizeof(unput_buf)) {
    unput_buf[unput_pos++] = c;
  }
}
