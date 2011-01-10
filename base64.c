
static char table[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char inv_table[] =
{
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  62, 127, 127, 127, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 127,
  127, 127, 127, 127, 127, 127, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 127, 127, 127,
  127, 127, 127, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127
};

int
base64_enclen(int x)
{
  return (x + 2 - ((x + 2) % 3)) / 3 * 4;
}

int
base64_declen(int x)
{
  return ((x * 6) + 7) >> 3;
}

int
base64_encode(int dst_len, char *dst, int src_len, char *src)
{
  unsigned long a;
  int n;

  for (n = 0;
       src_len >= 3 && dst_len >= 4; 
       src_len -= 3, src += 3, dst_len -= 4, dst += 4, n += 4) {
    a = src[2] | (src[1] << 8) | (src[0] << 16);
    dst[3] = table[a & 63];
    dst[2] = table[(a >> 6) & 63];
    dst[1] = table[(a >> 12) & 63];
    dst[0] = table[(a >> 18) & 63];
  }

  if (dst_len >= 4) {
    switch (src_len) {
    case 1:
      a = (src[0] << 16);
      dst[2] = dst[3] = '=';
      dst[1] = table[(a >> 12) & 63];
      dst[0] = table[(a >> 18) & 63];
      n += 1;
      break;
    case 2:
      a = (src[1] << 8) | (src[0] << 16);
      dst[3] = '=';
      dst[2] = table[(a >> 6) & 63];
      dst[1] = table[(a >> 12) & 63];
      dst[0] = table[(a >> 18) & 63];
      n += 2;
      break;
    }
  }
  return n;
}

int
base64_decode(int dst_len, char *dst, int src_len, char *src)
{
  unsigned long a;
  unsigned int b[4];
  int n, is_end;

  for (n = 0, is_end = 0;
       src_len >= 4 && dst_len >= 3 && !is_end;
       src_len -= 4, src += 4, dst_len -= 3, dst += 3, n += 3) {
    b[0] = inv_table[(unsigned char)src[0]];
    b[1] = inv_table[(unsigned char)src[1]];

    if (src[2] != '=') 
      b[2] = inv_table[(unsigned char)src[2]];
    else {
      n--;
      is_end = 1;
      b[2] = 0;
    }

    if (src[3] != '=')
      b[3] = inv_table[(unsigned char)src[3]];
    else {
      n--;
      is_end = 1;
      b[3] = 0;
    }

    a = b[3] | (b[2] << 6) | (b[1] << 12) | (b[0] << 18);
    dst[2] = a & 255;
    dst[1] = (a >> 8) & 255;
    dst[0] = (a >> 16) & 255;
  }
  return n;
}
