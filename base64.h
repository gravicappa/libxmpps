int base64_enclen(int x);
int base64_declen(int x);
int base64_encode(int dst_len, char *dst, int src_len, char *src);
int base64_decode(int dst_len, char *dst, int src_len, char *src);
