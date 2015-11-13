#include "php_ext_web_util.h"

PHP_MINIT_FUNCTION(php_ext_web_util);
PHP_MSHUTDOWN_FUNCTION(php_ext_web_util);
PHP_MINFO_FUNCTION(php_ext_web_util);

zend_module_entry php_ext_web_util_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "php_ext_web_util",
    NULL,
    PHP_MINIT(php_ext_web_util),
    PHP_MSHUTDOWN(php_ext_web_util),
    NULL,
    NULL,
    PHP_MINFO(php_ext_web_util),
#if ZEND_MODULE_API_NO >= 20010901
    "0.1",
#endif
    STANDARD_MODULE_PROPERTIES
};

#if COMPILE_DL_PHP_EXT_WEB_UTIL
ZEND_GET_MODULE(php_ext_web_util)
#endif

PHP_MINIT_FUNCTION(php_ext_web_util) {
    CLASS_ENTRY_FUNCTION_C(WebUtil_R3);
    CLASS_ENTRY_FUNCTION_C(WebUtil_http_parser);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(php_ext_web_util) {
    return SUCCESS;
}

PHP_MINFO_FUNCTION(php_ext_web_util) {
    php_info_print_table_start();
    php_info_print_table_header(2, "php_ext_web_util support", "enabled");
    php_info_print_table_end();
}
