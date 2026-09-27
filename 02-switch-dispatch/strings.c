/* You cannot switch on a string in C, and this is why: there is no instruction
   that compares bytes at two addresses. Every "string switch" in every language
   is one of the three shapes below. */
#include <string.h>

/* 1. Chain of compares. O(n) calls to strcmp. */
int by_strcmp(const char *s) {
    if (!strcmp(s, "get"))  return 1;
    if (!strcmp(s, "put"))  return 2;
    if (!strcmp(s, "post")) return 3;
    return -1;
}

/* 2. Discriminate on something cheap FIRST -- length, then first byte -- so
   most candidates die without a memory comparison. This is what a switch on
   strings compiles to in Java/C#/Go after hashing, and what you would hand-write. */
int by_shape(const char *s) {
    size_t n = strlen(s);
    switch (n) {
        case 3:
            if (s[0] == 'g' && s[1] == 'e' && s[2] == 't') return 1;
            if (s[0] == 'p' && s[1] == 'u' && s[2] == 't') return 2;
            return -1;
        case 4:
            if (!memcmp(s, "post", 4)) return 3;
            return -1;
    }
    return -1;
}

/* 3. Pack the bytes into an integer and switch on THAT. Now it really is a
   jump table, because it really is an integer. Four bytes, one comparison. */
int by_packing(const char *s) {
    unsigned k = 0;
    for (int i = 0; i < 4 && s[i]; i++) k |= (unsigned)(unsigned char)s[i] << (8 * i);
    switch (k) {
        case 0x00746567: return 1;   /* "get"  */
        case 0x00747570: return 2;   /* "put"  */
        case 0x74736f70: return 3;   /* "post" */
    }
    return -1;
}
