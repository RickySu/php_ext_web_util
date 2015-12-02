#ifndef _WEB_UTIL_HTTP_PARSER_H
#define _WEB_UTIL_HTTP_PARSER_H
#include "../php_ext_web_util.h"
#include "fcall_info.h"
#include "php_variables.h"
#include "util.h"
#include "bstring.h"
#include <http_parser.h>
#include <ext/json/php_json.h>

#ifndef PHP_JSON_PARSER_DEFAULT_DEPTH
#define PHP_JSON_PARSER_DEFAULT_DEPTH 512
#endif

#define CONTENT_TYPE_NONE       0
#define CONTENT_TYPE_URLENCODE  1
#define CONTENT_TYPE_JSONENCODE 2
#define CONTENT_TYPE_MULTIPART  3
#define TYPE_REQUEST    0
#define TYPE_RESPONSE   1

#define TYPE_MULTIPART_HEADER   0
#define TYPE_MULTIPART_CONTENT  1

#define UNSET_BSTRING(str) \
do{ \
    if(str){ \
        bstring_free(str); \
        str = NULL; \
    } \
} while(0)

CLASS_ENTRY_FUNCTION_D(WebUtil_http_parser);

ZEND_BEGIN_ARG_INFO(ARGINFO(WebUtil_http_parser, __construct), 0)
    ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(ARGINFO(WebUtil_http_parser, feed), 0)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(ARGINFO(WebUtil_http_parser, setOnHeaderParsedCallback), 0)
    ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(ARGINFO(WebUtil_http_parser, setOnBodyParsedCallback), 0)
    ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(ARGINFO(WebUtil_http_parser, setOnContentPieceCallback), 0)
    ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(ARGINFO(WebUtil_http_parser, setOnMultipartCallback), 0)
    ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

typedef struct http_parser_ext_s{
    http_parser parser;
    struct {
        bstring *url;
        bstring *header;
        bstring *field;
        bstring *body;
        bstring *multipartHeaderData;
        bstring *delimiter;
        bstring *delimiterClose;
        int contentType;
        int headerEnd;
        int multipartHeader;
        int multipartEnd;
    } parser_data;
    enum http_parser_type parserType;
    fcall_info_t onHeaderParsedCallback;
    fcall_info_t onBodyParsedCallback;
    fcall_info_t onContentPieceCallback;
    fcall_info_t onMultipartCallback;
    fcall_info_t parse_str;
    zval object;
    zend_object zo;
} http_parser_ext;

static zend_object_value createWebUtil_http_parserResource(zend_class_entry *class_type TSRMLS_DC);
void freeWebUtil_http_parserResource(void *object TSRMLS_DC);

PHP_METHOD(WebUtil_http_parser, __construct);
PHP_METHOD(WebUtil_http_parser, feed);
PHP_METHOD(WebUtil_http_parser, reset);
PHP_METHOD(WebUtil_http_parser, setOnHeaderParsedCallback);
PHP_METHOD(WebUtil_http_parser, setOnBodyParsedCallback);
PHP_METHOD(WebUtil_http_parser, setOnContentPieceCallback);
PHP_METHOD(WebUtil_http_parser, setOnMultipartCallback);

DECLARE_FUNCTION_ENTRY(WebUtil_http_parser) = {
    PHP_ME(WebUtil_http_parser, __construct, ARGINFO(WebUtil_http_parser, __construct), ZEND_ACC_PUBLIC)
    PHP_ME(WebUtil_http_parser, feed, ARGINFO(WebUtil_http_parser, feed), ZEND_ACC_PUBLIC)
    PHP_ME(WebUtil_http_parser, reset, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(WebUtil_http_parser, setOnHeaderParsedCallback, ARGINFO(WebUtil_http_parser, setOnHeaderParsedCallback), ZEND_ACC_PUBLIC)
    PHP_ME(WebUtil_http_parser, setOnBodyParsedCallback, ARGINFO(WebUtil_http_parser, setOnBodyParsedCallback), ZEND_ACC_PUBLIC)
    PHP_ME(WebUtil_http_parser, setOnContentPieceCallback, ARGINFO(WebUtil_http_parser, setOnContentPieceCallback), ZEND_ACC_PUBLIC)
    PHP_ME(WebUtil_http_parser, setOnMultipartCallback, ARGINFO(WebUtil_http_parser, setOnMultipartCallback), ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static zend_always_inline void releaseFunctionCache(http_parser_ext *resource TSRMLS_DC);
static zend_always_inline int multipartCallback(http_parser_ext *resource, bstring *data, int type TSRMLS_DC);
static zend_always_inline int sendData(http_parser_ext *parser, bstring *data TSRMLS_DC);
static zend_always_inline int flushBufferData(http_parser_ext *parser TSRMLS_DC);
static zend_always_inline zval *parseBody(http_parser_ext *resource TSRMLS_DC);
static zend_always_inline void parseContentType(http_parser_ext *resource TSRMLS_DC);
static zend_always_inline void parseCookie(http_parser_ext *resource, const char *cookie_field TSRMLS_DC);
static zend_always_inline void parseRequest(http_parser_ext *resource TSRMLS_DC);
static zend_always_inline void parseResponse(http_parser_ext *resource TSRMLS_DC);
static zend_always_inline void releaseParser(http_parser_ext *resource);

static void resetParserStatus(http_parser_ext *resource TSRMLS_DC);
static int on_message_begin(http_parser_ext *resource);
static int on_message_complete(http_parser_ext *resource);
static int on_headers_complete_request(http_parser_ext *resource);
static int on_headers_complete_response(http_parser_ext *resource);
static int on_status(http_parser_ext *resource, const char *buf, size_t len);
static int on_url(http_parser_ext *resource, const char *buf, size_t len);
static int on_header_field(http_parser_ext *resource, const char *buf, size_t len);
static int on_header_value(http_parser_ext *resource, const char *buf, size_t len);
static int on_response_body(http_parser_ext *resource, const char *buf, size_t len);
static int on_request_body(http_parser_ext *resource, const char *buf, size_t len);
#endif