#ifndef _WEB_UTIL_R3_H
#define _WEB_UTIL_R3_H
#include "../php_ext_web_util.h"
#include "util.h"
#include <r3/r3.h>

CLASS_ENTRY_FUNCTION_D(WebUtil_R3);

ZEND_BEGIN_ARG_INFO(ARGINFO(WebUtil_R3, addRoute), 0)
    ZEND_ARG_INFO(0, pattern)
    ZEND_ARG_INFO(0, method)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(ARGINFO(WebUtil_R3, match), 0)
    ZEND_ARG_INFO(0, uri)
    ZEND_ARG_INFO(0, method)
ZEND_END_ARG_INFO()

typedef struct web_util_r3_s{
    node *n;
    zend_object zo;
} web_util_r3_t;

static zend_object_value createWebUtil_R3Resource(zend_class_entry *class_type TSRMLS_DC);
void freeWebUtil_R3Resource(void *object TSRMLS_DC);

PHP_METHOD(WebUtil_R3, addRoute);
PHP_METHOD(WebUtil_R3, compile);
PHP_METHOD(WebUtil_R3, match);

DECLARE_FUNCTION_ENTRY(WebUtil_R3) = {
    PHP_ME(WebUtil_R3, addRoute, ARGINFO(WebUtil_R3, addRoute), ZEND_ACC_PUBLIC)
    PHP_ME(WebUtil_R3, compile, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(WebUtil_R3, match, ARGINFO(WebUtil_R3, match), ZEND_ACC_PUBLIC)
    PHP_FE_END
};

#endif