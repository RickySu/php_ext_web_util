#include "bstring.h"

void bstring_free(bstring *str){
    efree(str);
}

bstring *bstring_new(size_t len){
    bstring *str = emalloc(sizeof(bstring) + len);
    str->val[len] = '\0';
    str->len = len;
    return str;
}

bstring *bstring_make(const char *buf, size_t len){
    bstring *str = bstring_new(len);
    memcpy(str->val, buf, len);
    return str;
}

bstring *bstring_append(bstring *origin, const char *buf, size_t len){
    bstring *str;
    if(origin){
        str = bstring_new(origin->len + len);
        memcpy(str->val, origin->val, origin->len);
        memcpy(&str->val[origin->len], buf, len);
        bstring_free(origin);
        return str;
    }
    return bstring_make(buf, len);
}

void bstring_append_p(bstring **origin_p, const char *buf, size_t len){
    *origin_p = bstring_append(*origin_p, buf, len);
}