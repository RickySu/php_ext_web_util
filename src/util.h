#ifndef UTIL_H
#define	UTIL_H

#define COPY_C_STR(c_str, str, str_len) \
    memcpy(c_str, str, str_len); \
    c_str[str_len] = '\0'

#define MAKE_C_STR(c_str, str, str_len) \
    c_str = emalloc(str_len + 1); \
    COPY_C_STR(c_str, str, str_len)

#endif	/* UTIL_H */
