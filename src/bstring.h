#ifndef _BSTRING_H
#define _BSTRING_H
#include "../php_ext_web_util.h"

typedef struct bstring_s{
    size_t len;
    char val[1];
} bstring;

void bstring_cuthead(bstring *str, size_t pos);
void bstring_cuttail(bstring *str, size_t pos);
int bstring_find(bstring *str, bstring *findme);
int bstring_find_cstr(bstring *str, const char *findme, size_t findme_len);
void bstring_free(bstring *str);
void bstring_free_p(bstring **str);
bstring *bstring_new(size_t len);
bstring *bstring_make(const char *buf, size_t len);
bstring *bstring_copy(bstring *origin);
bstring *bstring_append(bstring *origin, const char *buf, size_t len);
void bstring_append_p(bstring **origin, const char *buf, size_t len);
#endif