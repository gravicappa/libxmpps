void set_input_callback(void *user, 
                        int (*callback)(void *user, int len, char *buf));
int input_getc(void);
void input_ungetc(int c);
