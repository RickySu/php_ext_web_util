#include "bstring.h"

int bstring_find(bstring *str, bstring *findme){
    return bstring_find_cstr(str, findme->val, findme->len);
}

int bstring_find_cstr(bstring *str, const char *findme, size_t findme_len){
    char *result;
    int end_pos = str->len - findme_len, i;

    if(str->len < findme_len){
        return -1;
    }

    for(i = 0; i <= end_pos; i++){
        if(memcmp(&str->val[i], findme, findme_len) == 0){
            return i;
        }
    }
    
    return -1;
}

void bstring_cuthead(bstring *str, size_t pos){                   
    memmove(str->val, &str->val[pos], str->len - pos);
    str->len -= pos;
    str->val[str->len] = '\0';
}
        
void bstring_cuttail(bstring *str, size_t pos){                   
    str->len = pos;
    str->val[str->len] = '\0';
}

void bstring_free_p(bstring **str){
    bstring_free(*str);
    str = NULL;
}

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

bstring *bstring_copy(bstring *origin){
    if(origin){
        return bstring_make(origin->val, origin->len);
    }
    return NULL;
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