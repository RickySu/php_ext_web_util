#ifndef _PHP_EXT_WEB_UTIL_H
#define _PHP_EXT_WEB_UTIL_H

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#ifdef ZTS
    #warning php_ext_uv module will *NEVER* be thread-safe
    #include <TSRM.h>
#endif

#include <php.h>
#include <r3.h>
#include "common.h"

extern zend_module_entry php_ext_web_util_module_entry;

DECLARE_CLASS_ENTRY(WebUtil_R3);
#endif
